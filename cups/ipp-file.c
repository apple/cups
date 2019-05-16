/*
 * IPP data file parsing functions.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "ipp-private.h"
#include "string-private.h"
#include "debug-internal.h"


/*
 * Local functions...
 */

static ipp_t	*parse_collection(_ipp_file_t *f, _ipp_vars_t *v, void *user_data);
static int	parse_value(_ipp_file_t *f, _ipp_vars_t *v, void *user_data, ipp_t *ipp, ipp_attribute_t **attr, int element);
static void	report_error(_ipp_file_t *f, _ipp_vars_t *v, void *user_data, const char *message, ...) _CUPS_FORMAT(4, 5);


/*
 * '_ippFileParse()' - Parse an IPP data file.
 */

ipp_t *					/* O - IPP attributes or @code NULL@ on failure */
_ippFileParse(
    _ipp_vars_t      *v,		/* I - Variables */
    const char       *filename,		/* I - Name of file to parse */
    void             *user_data)	/* I - User data pointer */
{
  _ipp_file_t	f;			/* IPP data file information */
  ipp_t		*attrs = NULL;		/* Active IPP message */
  ipp_attribute_t *attr = NULL;		/* Current attribute */
  char		token[1024];		/* Token string */
  ipp_t		*ignored = NULL;	/* Ignored attributes */


  DEBUG_printf(("_ippFileParse(v=%p, filename=\"%s\", user_data=%p)", (void *)v, filename, user_data));

 /*
  * Initialize file info...
  */

  memset(&f, 0, sizeof(f));
  f.filename = filename;
  f.linenum  = 1;

  if ((f.fp = cupsFileOpen(filename, "r")) == NULL)
  {
    DEBUG_printf(("1_ippFileParse: Unable to open \"%s\": %s", filename, strerror(errno)));
    return (0);
  }

 /*
  * Do the callback with a NULL token to setup any initial state...
  */

  (*v->tokencb)(&f, v, user_data, NULL);

 /*
  * Read data file, using the callback function as needed...
  */

  while (_ippFileReadToken(&f, token, sizeof(token)))
  {
    if (!_cups_strcasecmp(token, "DEFINE") || !_cups_strcasecmp(token, "DEFINE-DEFAULT"))
    {
      char	name[128],		/* Variable name */
		value[1024],		/* Variable value */
		temp[1024];		/* Temporary string */

      attr = NULL;

      if (_ippFileReadToken(&f, name, sizeof(name)) && _ippFileReadToken(&f, temp, sizeof(temp)))
      {
        if (_cups_strcasecmp(token, "DEFINE-DEFAULT") || !_ippVarsGet(v, name))
        {
	  _ippVarsExpand(v, value, temp, sizeof(value));
	  _ippVarsSet(v, name, value);
	}
      }
      else
      {
        report_error(&f, v, user_data, "Missing %s name and/or value on line %d of \"%s\".", token, f.linenum, f.filename);
        break;
      }
    }
    else if (f.attrs && !_cups_strcasecmp(token, "ATTR"))
    {
     /*
      * Attribute definition...
      */

      char	syntax[128],		/* Attribute syntax (value tag) */
		name[128];		/* Attribute name */
      ipp_tag_t	value_tag;		/* Value tag */

      attr = NULL;

      if (!_ippFileReadToken(&f, syntax, sizeof(syntax)))
      {
        report_error(&f, v, user_data, "Missing ATTR syntax on line %d of \"%s\".", f.linenum, f.filename);
	break;
      }
      else if ((value_tag = ippTagValue(syntax)) < IPP_TAG_UNSUPPORTED_VALUE)
      {
        report_error(&f, v, user_data, "Bad ATTR syntax \"%s\" on line %d of \"%s\".", syntax, f.linenum, f.filename);
	break;
      }

      if (!_ippFileReadToken(&f, name, sizeof(name)) || !name[0])
      {
        report_error(&f, v, user_data, "Missing ATTR name on line %d of \"%s\".", f.linenum, f.filename);
	break;
      }

      if (!v->attrcb || (*v->attrcb)(&f, user_data, name))
      {
       /*
        * Add this attribute...
        */

        attrs = f.attrs;
      }
      else
      {
       /*
        * Ignore this attribute...
        */

        if (!ignored)
          ignored = ippNew();

        attrs = ignored;
      }

      if (value_tag < IPP_TAG_INTEGER)
      {
       /*
	* Add out-of-band attribute - no value string needed...
	*/

        ippAddOutOfBand(attrs, f.group_tag, value_tag, name);
      }
      else
      {
       /*
        * Add attribute with one or more values...
        */

        attr = ippAddString(attrs, f.group_tag, value_tag, name, NULL, NULL);

        if (!parse_value(&f, v, user_data, attrs, &attr, 0))
          break;
      }

    }
    else if (attr && !_cups_strcasecmp(token, ","))
    {
     /*
      * Additional value...
      */

      if (!parse_value(&f, v, user_data, attrs, &attr, ippGetCount(attr)))
	break;
    }
    else
    {
     /*
      * Something else...
      */

      attr  = NULL;
      attrs = NULL;

      if (!(*v->tokencb)(&f, v, user_data, token))
        break;
    }
  }

 /*
  * Close the file and free ignored attributes, then return any attributes we
  * kept...
  */

  cupsFileClose(f.fp);
  ippDelete(ignored);

  return (f.attrs);
}


/*
 * '_ippFileReadToken()' - Read a token from an IPP data file.
 */

int					/* O - 1 on success, 0 on failure */
_ippFileReadToken(_ipp_file_t *f,	/* I - File to read from */
                  char        *token,	/* I - Token string buffer */
                  size_t      tokensize)/* I - Size of token string buffer */
{
  int	ch,				/* Character from file */
	quote = 0;			/* Quoting character */
  char	*tokptr = token,		/* Pointer into token buffer */
	*tokend = token + tokensize - 1;/* End of token buffer */


 /*
  * Skip whitespace and comments...
  */

  DEBUG_printf(("1_ippFileReadToken: linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));

  while ((ch = cupsFileGetChar(f->fp)) != EOF)
  {
    if (_cups_isspace(ch))
    {
     /*
      * Whitespace...
      */

      if (ch == '\n')
      {
        f->linenum ++;
        DEBUG_printf(("1_ippFileReadToken: LF in leading whitespace, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
      }
    }
    else if (ch == '#')
    {
     /*
      * Comment...
      */

      DEBUG_puts("1_ippFileReadToken: Skipping comment in leading whitespace...");

      while ((ch = cupsFileGetChar(f->fp)) != EOF)
      {
        if (ch == '\n')
          break;
      }

      if (ch == '\n')
      {
        f->linenum ++;
        DEBUG_printf(("1_ippFileReadToken: LF at end of comment, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
      }
      else
        break;
    }
    else
      break;
  }

  if (ch == EOF)
  {
    DEBUG_puts("1_ippFileReadToken: EOF");
    return (0);
  }

 /*
  * Read a token...
  */

  while (ch != EOF)
  {
    if (ch == '\n')
    {
      f->linenum ++;
      DEBUG_printf(("1_ippFileReadToken: LF in token, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
    }

    if (ch == quote)
    {
     /*
      * End of quoted text...
      */

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" at closing quote.", token));
      return (1);
    }
    else if (!quote && _cups_isspace(ch))
    {
     /*
      * End of unquoted text...
      */

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" before whitespace.", token));
      return (1);
    }
    else if (!quote && (ch == '\'' || ch == '\"'))
    {
     /*
      * Start of quoted text or regular expression...
      */

      if (ch == '<')
        quote = '>';
      else
        quote = ch;

      DEBUG_printf(("1_ippFileReadToken: Start of quoted string, quote=%c, pos=%ld", quote, (long)cupsFileTell(f->fp)));
    }
    else if (!quote && ch == '#')
    {
     /*
      * Start of comment...
      */

      cupsFileSeek(f->fp, cupsFileTell(f->fp) - 1);
      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" before comment.", token));
      return (1);
    }
    else if (!quote && (ch == '{' || ch == '}' || ch == ','))
    {
     /*
      * Delimiter...
      */

      if (tokptr > token)
      {
       /*
        * Return the preceding token first...
        */

	cupsFileSeek(f->fp, cupsFileTell(f->fp) - 1);
      }
      else
      {
       /*
        * Return this delimiter by itself...
        */

        *tokptr++ = (char)ch;
      }

      *tokptr = '\0';
      DEBUG_printf(("1_ippFileReadToken: Returning \"%s\".", token));
      return (1);
    }
    else
    {
      if (ch == '\\')
      {
       /*
        * Quoted character...
        */

        DEBUG_printf(("1_ippFileReadToken: Quoted character at pos=%ld", (long)cupsFileTell(f->fp)));

        if ((ch = cupsFileGetChar(f->fp)) == EOF)
        {
	  *token = '\0';
	  DEBUG_puts("1_ippFileReadToken: EOF");
	  return (0);
	}
	else if (ch == '\n')
	{
	  f->linenum ++;
	  DEBUG_printf(("1_ippFileReadToken: quoted LF, linenum=%d, pos=%ld", f->linenum, (long)cupsFileTell(f->fp)));
	}
	else if (ch == 'a')
	  ch = '\a';
	else if (ch == 'b')
	  ch = '\b';
	else if (ch == 'f')
	  ch = '\f';
	else if (ch == 'n')
	  ch = '\n';
	else if (ch == 'r')
	  ch = '\r';
	else if (ch == 't')
	  ch = '\t';
	else if (ch == 'v')
	  ch = '\v';
      }

      if (tokptr < tokend)
      {
       /*
	* Add to current token...
	*/

	*tokptr++ = (char)ch;
      }
      else
      {
       /*
	* Token too long...
	*/

	*tokptr = '\0';
	DEBUG_printf(("1_ippFileReadToken: Too long: \"%s\".", token));
	return (0);
      }
    }

   /*
    * Get the next character...
    */

    ch = cupsFileGetChar(f->fp);
  }

  *tokptr = '\0';
  DEBUG_printf(("1_ippFileReadToken: Returning \"%s\" at EOF.", token));

  return (tokptr > token);
}


/*
 * 'parse_collection()' - Parse an IPP collection value.
 */

static ipp_t *				/* O - Collection value or @code NULL@ on error */
parse_collection(
    _ipp_file_t      *f,		/* I - IPP data file */
    _ipp_vars_t      *v,		/* I - IPP variables */
    void             *user_data)	/* I - User data pointer */
{
  ipp_t		*col = ippNew();	/* Collection value */
  ipp_attribute_t *attr = NULL;		/* Current member attribute */
  char		token[1024];		/* Token string */


 /*
  * Parse the collection value...
  */

  while (_ippFileReadToken(f, token, sizeof(token)))
  {
    if (!_cups_strcasecmp(token, "}"))
    {
     /*
      * End of collection value...
      */

      break;
    }
    else if (!_cups_strcasecmp(token, "MEMBER"))
    {
     /*
      * Member attribute definition...
      */

      char	syntax[128],		/* Attribute syntax (value tag) */
		name[128];		/* Attribute name */
      ipp_tag_t	value_tag;		/* Value tag */

      attr = NULL;

      if (!_ippFileReadToken(f, syntax, sizeof(syntax)))
      {
        report_error(f, v, user_data, "Missing MEMBER syntax on line %d of \"%s\".", f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }
      else if ((value_tag = ippTagValue(syntax)) < IPP_TAG_UNSUPPORTED_VALUE)
      {
        report_error(f, v, user_data, "Bad MEMBER syntax \"%s\" on line %d of \"%s\".", syntax, f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }

      if (!_ippFileReadToken(f, name, sizeof(name)) || !name[0])
      {
        report_error(f, v, user_data, "Missing MEMBER name on line %d of \"%s\".", f->linenum, f->filename);
	ippDelete(col);
	col = NULL;
	break;
      }

      if (value_tag < IPP_TAG_INTEGER)
      {
       /*
	* Add out-of-band attribute - no value string needed...
	*/

        ippAddOutOfBand(col, IPP_TAG_ZERO, value_tag, name);
      }
      else
      {
       /*
        * Add attribute with one or more values...
        */

        attr = ippAddString(col, IPP_TAG_ZERO, value_tag, name, NULL, NULL);

        if (!parse_value(f, v, user_data, col, &attr, 0))
        {
	  ippDelete(col);
	  col = NULL;
          break;
	}
      }

    }
    else if (attr && !_cups_strcasecmp(token, ","))
    {
     /*
      * Additional value...
      */

      if (!parse_value(f, v, user_data, col, &attr, ippGetCount(attr)))
      {
	ippDelete(col);
	col = NULL;
	break;
      }
    }
    else
    {
     /*
      * Something else...
      */

      report_error(f, v, user_data, "Unknown directive \"%s\" on line %d of \"%s\".", token, f->linenum, f->filename);
      ippDelete(col);
      col  = NULL;
      attr = NULL;
      break;

    }
  }

  return (col);
}


/*
 * 'parse_value()' - Parse an IPP value.
 */

static int				/* O  - 1 on success or 0 on error */
parse_value(_ipp_file_t      *f,	/* I  - IPP data file */
            _ipp_vars_t      *v,	/* I  - IPP variables */
            void             *user_data,/* I  - User data pointer */
            ipp_t            *ipp,	/* I  - IPP message */
            ipp_attribute_t  **attr,	/* IO - IPP attribute */
            int              element)	/* I  - Element number */
{
  char		value[2049],		/* Value string */
		*valueptr,		/* Pointer into value string */
		temp[2049],		/* Temporary string */
		*tempptr;		/* Pointer into temporary string */
  size_t	valuelen;		/* Length of value */


  if (!_ippFileReadToken(f, temp, sizeof(temp)))
  {
    report_error(f, v, user_data, "Missing value on line %d of \"%s\".", f->linenum, f->filename);
    return (0);
  }

  _ippVarsExpand(v, value, temp, sizeof(value));

  switch (ippGetValueTag(*attr))
  {
    case IPP_TAG_BOOLEAN :
        return (ippSetBoolean(ipp, attr, element, !_cups_strcasecmp(value, "true")));
        break;

    case IPP_TAG_ENUM :
    case IPP_TAG_INTEGER :
        return (ippSetInteger(ipp, attr, element, (int)strtol(value, NULL, 0)));
        break;

    case IPP_TAG_DATE :
        {
          int	year,			/* Year */
		month,			/* Month */
		day,			/* Day of month */
		hour,			/* Hour */
		minute,			/* Minute */
		second,			/* Second */
		utc_offset = 0;		/* Timezone offset from UTC */
          ipp_uchar_t date[11];		/* dateTime value */

          if (*value == 'P')
          {
           /*
            * Time period...
            */

            time_t	curtime;	/* Current time in seconds */
            int		period = 0,	/* Current period value */
			saw_T = 0;	/* Saw time separator */

            curtime = time(NULL);

            for (valueptr = value + 1; *valueptr; valueptr ++)
            {
              if (isdigit(*valueptr & 255))
              {
                period = (int)strtol(valueptr, &valueptr, 10);

                if (!valueptr || period < 0)
                {
		  report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
		  return (0);
		}
              }

              if (*valueptr == 'Y')
              {
                curtime += 365 * 86400 * period;
                period  = 0;
              }
              else if (*valueptr == 'M')
              {
                if (saw_T)
                  curtime += 60 * period;
                else
                  curtime += 30 * 86400 * period;

                period = 0;
              }
              else if (*valueptr == 'D')
              {
                curtime += 86400 * period;
                period  = 0;
              }
              else if (*valueptr == 'H')
              {
                curtime += 3600 * period;
                period  = 0;
              }
              else if (*valueptr == 'S')
              {
                curtime += period;
                period = 0;
              }
              else if (*valueptr == 'T')
              {
                saw_T  = 1;
                period = 0;
              }
              else
	      {
		report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
		return (0);
	      }
	    }

	    return (ippSetDate(ipp, attr, element, ippTimeToDate(curtime)));
          }
          else if (sscanf(value, "%d-%d-%dT%d:%d:%d%d", &year, &month, &day, &hour, &minute, &second, &utc_offset) < 6)
          {
           /*
            * Date/time value did not parse...
            */

	    report_error(f, v, user_data, "Bad dateTime value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
          }

          date[0] = (ipp_uchar_t)(year >> 8);
          date[1] = (ipp_uchar_t)(year & 255);
          date[2] = (ipp_uchar_t)month;
          date[3] = (ipp_uchar_t)day;
          date[4] = (ipp_uchar_t)hour;
          date[5] = (ipp_uchar_t)minute;
          date[6] = (ipp_uchar_t)second;
          date[7] = 0;
          if (utc_offset < 0)
          {
            utc_offset = -utc_offset;
            date[8]    = (ipp_uchar_t)'-';
	  }
	  else
	  {
            date[8] = (ipp_uchar_t)'+';
	  }

          date[9]  = (ipp_uchar_t)(utc_offset / 100);
          date[10] = (ipp_uchar_t)(utc_offset % 100);

          return (ippSetDate(ipp, attr, element, date));
        }
        break;

    case IPP_TAG_RESOLUTION :
	{
	  int	xres,		/* X resolution */
		yres;		/* Y resolution */
	  char	*ptr;		/* Pointer into value */

	  xres = yres = (int)strtol(value, (char **)&ptr, 10);
	  if (ptr > value && xres > 0)
	  {
	    if (*ptr == 'x')
	      yres = (int)strtol(ptr + 1, (char **)&ptr, 10);
	  }

	  if (ptr <= value || xres <= 0 || yres <= 0 || !ptr || (_cups_strcasecmp(ptr, "dpi") && _cups_strcasecmp(ptr, "dpc") && _cups_strcasecmp(ptr, "dpcm") && _cups_strcasecmp(ptr, "other")))
	  {
	    report_error(f, v, user_data, "Bad resolution value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
	  }

	  if (!_cups_strcasecmp(ptr, "dpi"))
	    return (ippSetResolution(ipp, attr, element, IPP_RES_PER_INCH, xres, yres));
	  else if (!_cups_strcasecmp(ptr, "dpc") || !_cups_strcasecmp(ptr, "dpcm"))
	    return (ippSetResolution(ipp, attr, element, IPP_RES_PER_CM, xres, yres));
	  else
	    return (ippSetResolution(ipp, attr, element, (ipp_res_t)0, xres, yres));
	}
	break;

    case IPP_TAG_RANGE :
	{
	  int	lower,			/* Lower value */
		upper;			/* Upper value */

          if (sscanf(value, "%d-%d", &lower, &upper) != 2)
          {
	    report_error(f, v, user_data, "Bad rangeOfInteger value \"%s\" on line %d of \"%s\".", value, f->linenum, f->filename);
	    return (0);
	  }

	  return (ippSetRange(ipp, attr, element, lower, upper));
	}
	break;

    case IPP_TAG_STRING :
        valuelen = strlen(value);

        if (value[0] == '<' && value[strlen(value) - 1] == '>')
        {
          if (valuelen & 1)
          {
	    report_error(f, v, user_data, "Bad octetString value on line %d of \"%s\".", f->linenum, f->filename);
	    return (0);
          }

          valueptr = value + 1;
          tempptr  = temp;

          while (*valueptr && *valueptr != '>')
          {
	    if (!isxdigit(valueptr[0] & 255) || !isxdigit(valueptr[1] & 255))
	    {
	      report_error(f, v, user_data, "Bad octetString value on line %d of \"%s\".", f->linenum, f->filename);
	      return (0);
	    }

            if (valueptr[0] >= '0' && valueptr[0] <= '9')
              *tempptr = (char)((valueptr[0] - '0') << 4);
	    else
              *tempptr = (char)((tolower(valueptr[0]) - 'a' + 10) << 4);

            if (valueptr[1] >= '0' && valueptr[1] <= '9')
              *tempptr |= (valueptr[1] - '0');
	    else
              *tempptr |= (tolower(valueptr[1]) - 'a' + 10);

            tempptr ++;
          }

          return (ippSetOctetString(ipp, attr, element, temp, (int)(tempptr - temp)));
        }
        else
          return (ippSetOctetString(ipp, attr, element, value, (int)valuelen));
        break;

    case IPP_TAG_TEXTLANG :
    case IPP_TAG_NAMELANG :
    case IPP_TAG_TEXT :
    case IPP_TAG_NAME :
    case IPP_TAG_KEYWORD :
    case IPP_TAG_URI :
    case IPP_TAG_URISCHEME :
    case IPP_TAG_CHARSET :
    case IPP_TAG_LANGUAGE :
    case IPP_TAG_MIMETYPE :
        return (ippSetString(ipp, attr, element, value));
        break;

    case IPP_TAG_BEGIN_COLLECTION :
        {
          int	status;			/* Add status */
          ipp_t *col;			/* Collection value */

          if (strcmp(value, "{"))
          {
	    report_error(f, v, user_data, "Bad collection value on line %d of \"%s\".", f->linenum, f->filename);
	    return (0);
          }

          if ((col = parse_collection(f, v, user_data)) == NULL)
            return (0);

	  status = ippSetCollection(ipp, attr, element, col);
	  ippDelete(col);

	  return (status);
	}
	break;

    default :
        report_error(f, v, user_data, "Unsupported value on line %d of \"%s\".", f->linenum, f->filename);
        return (0);
  }

  return (1);
}


/*
 * 'report_error()' - Report an error.
 */

static void
report_error(
    _ipp_file_t *f,			/* I - IPP data file */
    _ipp_vars_t *v,			/* I - Error callback function, if any */
    void        *user_data,		/* I - User data pointer */
    const char  *message,		/* I - Printf-style message */
    ...)				/* I - Additional arguments as needed */
{
  char		buffer[8192];		/* Formatted string */
  va_list	ap;			/* Argument pointer */


  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  if (v->errorcb)
    (*v->errorcb)(f, user_data, buffer);
  else
    fprintf(stderr, "%s\n", buffer);
}
