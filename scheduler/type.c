/*
 * "$Id: type.c 12578 2015-03-30 19:07:29Z msweet $"
 *
 * MIME typing routines for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2006 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/string-private.h>
#include <cups/debug-private.h>
#include <locale.h>
#include "mime.h"


/*
 * Local types...
 */

typedef struct _mime_filebuf_s		/**** File buffer for MIME typing ****/
{
  cups_file_t	*fp;			/* File pointer */
  int		offset,			/* Offset in file */
		length;			/* Length of buffered data */
  unsigned char	buffer[MIME_MAX_BUFFER];/* Buffered data */
} _mime_filebuf_t;


/*
 * Local functions...
 */

static int	mime_compare_types(mime_type_t *t0, mime_type_t *t1);
static int	mime_check_rules(const char *filename, _mime_filebuf_t *fb,
		                 mime_magic_t *rules);
static int	mime_patmatch(const char *s, const char *pat);


/*
 * Local globals...
 */

#ifdef DEBUG
static const char * const debug_ops[] =
		{			/* Test names... */
		  "NOP",		/* No operation */
		  "AND",		/* Logical AND of all children */
		  "OR",			/* Logical OR of all children */
		  "MATCH",		/* Filename match */
		  "ASCII",		/* ASCII characters in range */
		  "PRINTABLE",		/* Printable characters (32-255) */
		  "STRING",		/* String matches */
		  "CHAR",		/* Character/byte matches */
		  "SHORT",		/* Short/16-bit word matches */
		  "INT",		/* Integer/32-bit word matches */
		  "LOCALE",		/* Current locale matches string */
		  "CONTAINS",		/* File contains a string */
		  "ISTRING"		/* Case-insensitive string matches */
		};
#endif /* DEBUG */


/*
 * 'mimeAddType()' - Add a MIME type to a database.
 */

mime_type_t *				/* O - New (or existing) MIME type */
mimeAddType(mime_t     *mime,		/* I - MIME database */
            const char *super,		/* I - Super-type name */
	    const char *type)		/* I - Type name */
{
  mime_type_t	*temp;			/* New MIME type */
  size_t	typelen;		/* Length of type name */


  DEBUG_printf(("mimeAddType(mime=%p, super=\"%s\", type=\"%s\")", mime, super,
                type));

 /*
  * Range check input...
  */

  if (!mime || !super || !type)
  {
    DEBUG_puts("1mimeAddType: Returning NULL (bad arguments).");
    return (NULL);
  }

 /*
  * See if the type already exists; if so, return the existing type...
  */

  if ((temp = mimeType(mime, super, type)) != NULL)
  {
    DEBUG_printf(("1mimeAddType: Returning %p (existing).", temp));
    return (temp);
  }

 /*
  * The type doesn't exist; add it...
  */

  if (!mime->types)
    mime->types = cupsArrayNew((cups_array_func_t)mime_compare_types, NULL);

  if (!mime->types)
  {
    DEBUG_puts("1mimeAddType: Returning NULL (no types).");
    return (NULL);
  }

  typelen = strlen(type) + 1;

  if ((temp = calloc(1, sizeof(mime_type_t) - MIME_MAX_TYPE + typelen)) == NULL)
  {
    DEBUG_puts("1mimeAddType: Returning NULL (out of memory).");
    return (NULL);
  }

  strlcpy(temp->super, super, sizeof(temp->super));
  memcpy(temp->type, type, typelen);
  temp->priority = 100;

  cupsArrayAdd(mime->types, temp);

  DEBUG_printf(("1mimeAddType: Returning %p (new).", temp));
  return (temp);
}


/*
 * 'mimeAddTypeRule()' - Add a detection rule for a file type.
 */

int					/* O - 0 on success, -1 on failure */
mimeAddTypeRule(mime_type_t *mt,	/* I - Type to add to */
                const char  *rule)	/* I - Rule to add */
{
  int		num_values,		/* Number of values seen */
		op,			/* Operation code */
		logic,			/* Logic for next rule */
		invert;			/* Invert following rule? */
  char		name[255],		/* Name in rule string */
		value[3][255],		/* Value in rule string */
		*ptr,			/* Position in name or value */
		quote;			/* Quote character */
  int		length[3];		/* Length of each parameter */
  mime_magic_t	*temp,			/* New rule */
		*current;  		/* Current rule */


  DEBUG_printf(("mimeAddTypeRule(mt=%p(%s/%s), rule=\"%s\")", mt,
                mt ? mt->super : "???", mt ? mt->type : "???", rule));

 /*
  * Range check input...
  */

  if (!mt || !rule)
    return (-1);

 /*
  * Find the last rule in the top-level of the rules tree.
  */

  for (current = mt->rules; current; current = current->next)
    if (!current->next)
      break;

 /*
  * Parse the rules string.  Most rules are either a file extension or a
  * comparison function:
  *
  *    extension
  *    function(parameters)
  */

  logic  = MIME_MAGIC_NOP;
  invert = 0;

  while (*rule != '\0')
  {
    while (isspace(*rule & 255))
      rule ++;

    if (*rule == '(')
    {
      DEBUG_puts("1mimeAddTypeRule: New parenthesis group");
      logic = MIME_MAGIC_NOP;
      rule ++;
    }
    else if (*rule == ')')
    {
      DEBUG_puts("1mimeAddTypeRule: Close paren...");
      if (current == NULL || current->parent == NULL)
        return (-1);

      current = current->parent;

      if (current->parent == NULL)
        logic = MIME_MAGIC_OR;
      else
        logic = current->parent->op;

      rule ++;
    }
    else if (*rule == '+' && current != NULL)
    {
      if (logic != MIME_MAGIC_AND &&
          current != NULL && current->prev != NULL)
      {
       /*
        * OK, we have more than 1 rule in the current tree level...  Make a
	* new group tree and move the previous rule to it...
	*/

	if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	  return (-1);

        temp->op            = MIME_MAGIC_AND;
        temp->child         = current;
        temp->parent        = current->parent;
	current->prev->next = temp;
	temp->prev          = current->prev;

        current->prev   = NULL;
	current->parent = temp;

        DEBUG_printf(("1mimeAddTypeRule: Creating new AND group %p.", temp));
      }
      else if (current->parent)
      {
        DEBUG_printf(("1mimeAddTypeRule: Setting group %p op to AND.",
	              current->parent));
        current->parent->op = MIME_MAGIC_AND;
      }

      logic = MIME_MAGIC_AND;
      rule ++;
    }
    else if (*rule == ',')
    {
      if (logic != MIME_MAGIC_OR && current != NULL)
      {
       /*
        * OK, we have two possibilities; either this is the top-level rule or
	* we have a bunch of AND rules at this level.
	*/

	if (current->parent == NULL)
	{
	 /*
	  * This is the top-level rule; we have to move *all* of the AND rules
	  * down a level, as AND has precedence over OR.
	  */

	  if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	    return (-1);

          DEBUG_printf(("1mimeAddTypeRule: Creating new AND group %p inside OR "
	                "group.", temp));

          while (current->prev != NULL)
	  {
	    current->parent = temp;
	    current         = current->prev;
	  }

          current->parent = temp;
          temp->op        = MIME_MAGIC_AND;
          temp->child     = current;

          mt->rules = current = temp;
	}
	else
	{
	 /*
	  * This isn't the top rule, so go up one level...
	  */

          DEBUG_puts("1mimeAddTypeRule: Going up one level.");
	  current = current->parent;
	}
      }

      logic = MIME_MAGIC_OR;
      rule ++;
    }
    else if (*rule == '!')
    {
      DEBUG_puts("1mimeAddTypeRule: NOT");
      invert = 1;
      rule ++;
    }
    else if (isalnum(*rule & 255))
    {
     /*
      * Read an extension name or a function...
      */

      ptr = name;
      while (isalnum(*rule & 255) && (size_t)(ptr - name) < (sizeof(name) - 1))
        *ptr++ = *rule++;

      *ptr = '\0';

      if (*rule == '(')
      {
       /*
        * Read function parameters...
	*/

	rule ++;
	for (num_values = 0;
	     num_values < (int)(sizeof(value) / sizeof(value[0]));
	     num_values ++)
	{
	  ptr = value[num_values];

	  while ((size_t)(ptr - value[num_values]) < (sizeof(value[0]) - 1) &&
	         *rule != '\0' && *rule != ',' && *rule != ')')
	  {
	    if (isspace(*rule & 255))
	    {
	     /*
	      * Ignore whitespace...
	      */

	      rule ++;
	      continue;
	    }
	    else if (*rule == '\"' || *rule == '\'')
	    {
	     /*
	      * Copy quoted strings literally...
	      */

	      quote = *rule++;

	      while (*rule != '\0' && *rule != quote &&
	             (size_t)(ptr - value[num_values]) < (sizeof(value[0]) - 1))
	        *ptr++ = *rule++;

              if (*rule == quote)
	        rule ++;
	      else
		return (-1);
	    }
	    else if (*rule == '<')
	    {
	      rule ++;

	      while (*rule != '>' && *rule != '\0' &&
	             (size_t)(ptr - value[num_values]) < (sizeof(value[0]) - 1))
	      {
	        if (isxdigit(rule[0] & 255) && isxdigit(rule[1] & 255))
		{
		  if (isdigit(*rule))
		    *ptr = (char)((*rule++ - '0') << 4);
		  else
		    *ptr = (char)((tolower(*rule++) - 'a' + 10) << 4);

		  if (isdigit(*rule))
		    *ptr++ |= *rule++ - '0';
		  else
		    *ptr++ |= tolower(*rule++) - 'a' + 10;
		}
		else
	          return (-1);
	      }

              if (*rule == '>')
	        rule ++;
	      else
		return (-1);
	    }
	    else
	      *ptr++ = *rule++;
	  }

          *ptr = '\0';
	  length[num_values] = ptr - value[num_values];

          if (*rule != ',')
	  {
	    num_values ++;
	    break;
	  }

          rule ++;
	}

        if (*rule != ')')
	  return (-1);

	rule ++;

       /*
        * Figure out the function...
	*/

        if (!strcmp(name, "match"))
	  op = MIME_MAGIC_MATCH;
	else if (!strcmp(name, "ascii"))
	  op = MIME_MAGIC_ASCII;
	else if (!strcmp(name, "printable"))
	  op = MIME_MAGIC_PRINTABLE;
	else if (!strcmp(name, "regex"))
	  op = MIME_MAGIC_REGEX;
	else if (!strcmp(name, "string"))
	  op = MIME_MAGIC_STRING;
	else if (!strcmp(name, "istring"))
	  op = MIME_MAGIC_ISTRING;
	else if (!strcmp(name, "char"))
	  op = MIME_MAGIC_CHAR;
	else if (!strcmp(name, "short"))
	  op = MIME_MAGIC_SHORT;
	else if (!strcmp(name, "int"))
	  op = MIME_MAGIC_INT;
	else if (!strcmp(name, "locale"))
	  op = MIME_MAGIC_LOCALE;
	else if (!strcmp(name, "contains"))
	  op = MIME_MAGIC_CONTAINS;
	else if (!strcmp(name, "priority") && num_values == 1)
	{
	  mt->priority = atoi(value[0]);
	  continue;
	}
	else
	  return (-1);
      }
      else
      {
       /*
        * This is just a filename match on the extension...
	*/

	snprintf(value[0], sizeof(value[0]), "*.%s", name);
	length[0]  = (int)strlen(value[0]);
	op         = MIME_MAGIC_MATCH;
      }

     /*
      * Add a rule for this operation.
      */

      if ((temp = calloc(1, sizeof(mime_magic_t))) == NULL)
	return (-1);

      temp->invert = (short)invert;
      if (current != NULL)
      {
	temp->parent  = current->parent;
	current->next = temp;
      }
      else
        mt->rules = temp;

      temp->prev = current;

      if (logic == MIME_MAGIC_NOP)
      {
       /*
        * Add parenthetical grouping...
	*/

        DEBUG_printf(("1mimeAddTypeRule: Making new OR group %p for "
	              "parenthesis.", temp));

        temp->op = MIME_MAGIC_OR;

	if ((temp->child = calloc(1, sizeof(mime_magic_t))) == NULL)
	  return (-1);

	temp->child->parent = temp;
	temp->child->invert = temp->invert;
	temp->invert        = 0;

	temp  = temp->child;
        logic = MIME_MAGIC_OR;
      }

      DEBUG_printf(("1mimeAddTypeRule: Adding %p: %s, op=MIME_MAGIC_%s(%d), "
		    "logic=MIME_MAGIC_%s, invert=%d.", temp, name,
		    debug_ops[op], op, debug_ops[logic], invert));

     /*
      * Fill in data for the rule...
      */

      current  = temp;
      temp->op = (short)op;
      invert   = 0;

      switch (op)
      {
        case MIME_MAGIC_MATCH :
	    if ((size_t)length[0] > (sizeof(temp->value.matchv) - 1))
	      return (-1);
	    strlcpy(temp->value.matchv, value[0], sizeof(temp->value.matchv));
	    break;
	case MIME_MAGIC_ASCII :
	case MIME_MAGIC_PRINTABLE :
	    temp->offset = strtol(value[0], NULL, 0);
	    temp->length = strtol(value[1], NULL, 0);
	    if (temp->length > MIME_MAX_BUFFER)
	      temp->length = MIME_MAX_BUFFER;
	    break;
	case MIME_MAGIC_REGEX :
	    temp->offset = strtol(value[0], NULL, 0);
	    temp->length = MIME_MAX_BUFFER;
	    if (regcomp(&(temp->value.rev), value[1], REG_NOSUB | REG_EXTENDED))
	      return (-1);
	    break;
	case MIME_MAGIC_STRING :
	case MIME_MAGIC_ISTRING :
	    temp->offset = strtol(value[0], NULL, 0);
	    if ((size_t)length[1] > sizeof(temp->value.stringv))
	      return (-1);
	    temp->length = length[1];
	    memcpy(temp->value.stringv, value[1], (size_t)length[1]);
	    break;
	case MIME_MAGIC_CHAR :
	    temp->offset = strtol(value[0], NULL, 0);
	    if (length[1] == 1)
	      temp->value.charv = (unsigned char)value[1][0];
	    else
	      temp->value.charv = (unsigned char)strtol(value[1], NULL, 0);

	    DEBUG_printf(("1mimeAddTypeRule: CHAR(%d,0x%02x)", temp->offset,
	                  temp->value.charv));
	    break;
	case MIME_MAGIC_SHORT :
	    temp->offset       = strtol(value[0], NULL, 0);
	    temp->value.shortv = (unsigned short)strtol(value[1], NULL, 0);
	    break;
	case MIME_MAGIC_INT :
	    temp->offset     = strtol(value[0], NULL, 0);
	    temp->value.intv = (unsigned)strtol(value[1], NULL, 0);
	    break;
	case MIME_MAGIC_LOCALE :
	    if ((size_t)length[0] > (sizeof(temp->value.localev) - 1))
	      return (-1);

	    strlcpy(temp->value.localev, value[0], sizeof(temp->value.localev));
	    break;
	case MIME_MAGIC_CONTAINS :
	    temp->offset = strtol(value[0], NULL, 0);
	    temp->region = strtol(value[1], NULL, 0);
	    if ((size_t)length[2] > sizeof(temp->value.stringv))
	      return (-1);
	    temp->length = length[2];
	    memcpy(temp->value.stringv, value[2], (size_t)length[2]);
	    break;
      }
    }
    else
      break;
  }

  return (0);
}


/*
 * 'mimeFileType()' - Determine the type of a file.
 */

mime_type_t *				/* O - Type of file */
mimeFileType(mime_t     *mime,		/* I - MIME database */
             const char *pathname,	/* I - Name of file to check on disk */
	     const char *filename,	/* I - Original filename or NULL */
	     int        *compression)	/* O - Is the file compressed? */
{
  _mime_filebuf_t	fb;		/* File buffer */
  const char		*base;		/* Base filename of file */
  mime_type_t		*type,		/* File type */
			*best;		/* Best match */


  DEBUG_printf(("mimeFileType(mime=%p, pathname=\"%s\", filename=\"%s\", "
                "compression=%p)", mime, pathname, filename, compression));

 /*
  * Range check input parameters...
  */

  if (!mime || !pathname)
  {
    DEBUG_puts("1mimeFileType: Returning NULL.");
    return (NULL);
  }

 /*
  * Try to open the file...
  */

  if ((fb.fp = cupsFileOpen(pathname, "r")) == NULL)
  {
    DEBUG_printf(("1mimeFileType: Unable to open \"%s\": %s", pathname,
                  strerror(errno)));
    DEBUG_puts("1mimeFileType: Returning NULL.");
    return (NULL);
  }

 /*
  * Then preload the first MIME_MAX_BUFFER bytes of the file into the file
  * buffer, returning an error if we can't read anything...
  */

  fb.offset = 0;
  fb.length = (int)cupsFileRead(fb.fp, (char *)fb.buffer, MIME_MAX_BUFFER);

  if (fb.length <= 0)
  {
    DEBUG_printf(("1mimeFileType: Unable to read from \"%s\": %s", pathname, strerror(errno)));
    DEBUG_puts("1mimeFileType: Returning NULL.");

    cupsFileClose(fb.fp);

    return (NULL);
  }

 /*
  * Figure out the base filename (without directory portion)...
  */

  if (filename)
  {
    if ((base = strrchr(filename, '/')) != NULL)
      base ++;
    else
      base = filename;
  }
  else if ((base = strrchr(pathname, '/')) != NULL)
    base ++;
  else
    base = pathname;

 /*
  * Then check it against all known types...
  */

  for (type = (mime_type_t *)cupsArrayFirst(mime->types), best = NULL;
       type;
       type = (mime_type_t *)cupsArrayNext(mime->types))
    if (mime_check_rules(base, &fb, type->rules))
    {
      if (!best || type->priority > best->priority)
        best = type;
    }

 /*
  * Finally, close the file and return a match (if any)...
  */

  if (compression)
  {
    *compression = cupsFileCompression(fb.fp);
    DEBUG_printf(("1mimeFileType: *compression=%d", *compression));
  }

  cupsFileClose(fb.fp);

  DEBUG_printf(("1mimeFileType: Returning %p(%s/%s).", best,
                best ? best->super : "???", best ? best->type : "???"));
  return (best);
}


/*
 * 'mimeType()' - Lookup a file type.
 */

mime_type_t *				/* O - Matching file type definition */
mimeType(mime_t     *mime,		/* I - MIME database */
         const char *super,		/* I - Super-type name */
	 const char *type)		/* I - Type name */
{
  mime_type_t	key,			/* MIME type search key */
		*mt;			/* Matching type */


  DEBUG_printf(("mimeType(mime=%p, super=\"%s\", type=\"%s\")", mime, super,
                type));

 /*
  * Range check input...
  */

  if (!mime || !super || !type)
  {
    DEBUG_puts("1mimeType: Returning NULL.");
    return (NULL);
  }

 /*
  * Lookup the type in the array...
  */

  strlcpy(key.super, super, sizeof(key.super));
  strlcpy(key.type, type, sizeof(key.type));

  mt = (mime_type_t *)cupsArrayFind(mime->types, &key);
  DEBUG_printf(("1mimeType: Returning %p.", mt));
  return (mt);
}


/*
 * 'mime_compare_types()' - Compare two MIME super/type names.
 */

static int				/* O - Result of comparison */
mime_compare_types(mime_type_t *t0,	/* I - First type */
                   mime_type_t *t1)	/* I - Second type */
{
  int	i;				/* Result of comparison */


  if ((i = _cups_strcasecmp(t0->super, t1->super)) == 0)
    i = _cups_strcasecmp(t0->type, t1->type);

  return (i);
}


/*
 * 'mime_check_rules()' - Check each rule in a list.
 */

static int				/* O - 1 if match, 0 if no match */
mime_check_rules(
    const char      *filename,		/* I - Filename */
    _mime_filebuf_t *fb,		/* I - File to check */
    mime_magic_t    *rules)		/* I - Rules to check */
{
  int		n;			/* Looping var */
  int		region;			/* Region to look at */
  int		logic,			/* Logic to apply */
		result;			/* Result of test */
  unsigned	intv;			/* Integer value */
  short		shortv;			/* Short value */
  unsigned char	*bufptr;		/* Pointer into buffer */


  DEBUG_printf(("4mime_check_rules(filename=\"%s\", fb=%p, rules=%p)", filename,
                fb, rules));

  if (rules == NULL)
    return (0);

  if (rules->parent == NULL)
    logic = MIME_MAGIC_OR;
  else
    logic = rules->parent->op;

  result = 0;

  while (rules != NULL)
  {
   /*
    * Compute the result of this rule...
    */

    switch (rules->op)
    {
      case MIME_MAGIC_MATCH :
          result = mime_patmatch(filename, rules->value.matchv);
	  break;

      case MIME_MAGIC_ASCII :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->length) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_ASCII fb->length=%d", fb->length));
	  }

         /*
	  * Test for ASCII printable characters plus standard control chars.
	  */

	  if ((rules->offset + rules->length) > (fb->offset + fb->length))
	    n = fb->offset + fb->length - rules->offset;
	  else
	    n = rules->length;

          bufptr = fb->buffer + rules->offset - fb->offset;
	  while (n > 0)
	    if ((*bufptr >= 32 && *bufptr <= 126) ||
	        (*bufptr >= 8 && *bufptr <= 13) ||
		*bufptr == 26 || *bufptr == 27)
	    {
	      n --;
	      bufptr ++;
	    }
	    else
	      break;

	  result = (n == 0);
	  break;

      case MIME_MAGIC_PRINTABLE :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->length) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_PRINTABLE fb->length=%d", fb->length));
	  }

         /*
	  * Test for 8-bit printable characters plus standard control chars.
	  */

	  if ((rules->offset + rules->length) > (fb->offset + fb->length))
	    n = fb->offset + fb->length - rules->offset;
	  else
	    n = rules->length;

          bufptr = fb->buffer + rules->offset - fb->offset;

	  while (n > 0)
	    if (*bufptr >= 128 ||
	        (*bufptr >= 32 && *bufptr <= 126) ||
	        (*bufptr >= 8 && *bufptr <= 13) ||
		*bufptr == 26 || *bufptr == 27)
	    {
	      n --;
	      bufptr ++;
	    }
	    else
	      break;

	  result = (n == 0);
	  break;

      case MIME_MAGIC_REGEX :
          DEBUG_printf(("5mime_check_rules: regex(%d, \"%s\")", rules->offset,
	                rules->value.stringv));

         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->length) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_REGEX fb->length=%d", fb->length));

            DEBUG_printf(("5mime_check_rules: loaded %d byte fb->buffer at %d, starts "
	                  "with \"%c%c%c%c\".",
	                  fb->length, fb->offset, fb->buffer[0], fb->buffer[1],
			  fb->buffer[2], fb->buffer[3]));
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

          if (fb->length > 0)
          {
            char temp[MIME_MAX_BUFFER + 1];
					/* Temporary buffer */

            memcpy(temp, fb->buffer, (size_t)fb->length);
            temp[fb->length] = '\0';
            result = !regexec(&(rules->value.rev), temp, 0, NULL, 0);
          }

          DEBUG_printf(("5mime_check_rules: result=%d", result));
	  break;

      case MIME_MAGIC_STRING :
          DEBUG_printf(("5mime_check_rules: string(%d, \"%s\")", rules->offset,
	                rules->value.stringv));

         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->length) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_STRING fb->length=%d", fb->length));

            DEBUG_printf(("5mime_check_rules: loaded %d byte fb->buffer at %d, starts "
	                  "with \"%c%c%c%c\".",
	                  fb->length, fb->offset, fb->buffer[0], fb->buffer[1],
			  fb->buffer[2], fb->buffer[3]));
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

	  if ((rules->offset + rules->length) > (fb->offset + fb->length))
	    result = 0;
	  else
            result = !memcmp(fb->buffer + rules->offset - fb->offset, rules->value.stringv, (size_t)rules->length);
          DEBUG_printf(("5mime_check_rules: result=%d", result));
	  break;

      case MIME_MAGIC_ISTRING :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->length) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_ISTRING fb->length=%d", fb->length));
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

	  if ((rules->offset + rules->length) > (fb->offset + fb->length))
	    result = 0;
	  else
            result = !_cups_strncasecmp((char *)fb->buffer + rules->offset - fb->offset, rules->value.stringv, (size_t)rules->length);
	  break;

      case MIME_MAGIC_CHAR :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset)
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_CHAR fb->length=%d", fb->length));
	  }

	 /*
	  * Compare the character values; if the file is too short, it
	  * can't match...
	  */

	  if (fb->length < 1)
	    result = 0;
	  else
	    result = (fb->buffer[rules->offset - fb->offset] ==
	                  rules->value.charv);
	  break;

      case MIME_MAGIC_SHORT :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + 2) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_SHORT fb->length=%d", fb->length));
	  }

	 /*
	  * Compare the short values; if the file is too short, it
	  * can't match...
	  */

	  if (fb->length < 2)
	    result = 0;
	  else
	  {
	    bufptr = fb->buffer + rules->offset - fb->offset;
	    shortv = (short)((bufptr[0] << 8) | bufptr[1]);
	    result = (shortv == rules->value.shortv);
	  }
	  break;

      case MIME_MAGIC_INT :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + 4) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_INT fb->length=%d", fb->length));
	  }

	 /*
	  * Compare the int values; if the file is too short, it
	  * can't match...
	  */

	  if (fb->length < 4)
	    result = 0;
	  else
	  {
	    bufptr = fb->buffer + rules->offset - fb->offset;
	    intv   = (unsigned)((((((bufptr[0] << 8) | bufptr[1]) << 8) | bufptr[2]) << 8) | bufptr[3]);
	    result = (intv == rules->value.intv);
	  }
	  break;

      case MIME_MAGIC_LOCALE :
#if defined(WIN32) || defined(__EMX__) || defined(__APPLE__)
          result = !strcmp(rules->value.localev, setlocale(LC_ALL, ""));
#else
          result = !strcmp(rules->value.localev, setlocale(LC_MESSAGES, ""));
#endif /* __APPLE__ */
	  break;

      case MIME_MAGIC_CONTAINS :
         /*
	  * Load the buffer if necessary...
	  */

          if (fb->offset < 0 || rules->offset < fb->offset ||
	      (rules->offset + rules->region) > (fb->offset + fb->length))
	  {
	   /*
	    * Reload file buffer...
	    */

            cupsFileSeek(fb->fp, rules->offset);
	    fb->length = cupsFileRead(fb->fp, (char *)fb->buffer,
	                              sizeof(fb->buffer));
	    fb->offset = rules->offset;

	    DEBUG_printf(("4mime_check_rules: MIME_MAGIC_CONTAINS fb->length=%d", fb->length));
	  }

         /*
	  * Compare the buffer against the string.  If the file is too
	  * short then don't compare - it can't match...
	  */

	  if ((rules->offset + rules->length) > (fb->offset + fb->length))
	    result = 0;
	  else
	  {
	    if (fb->length > rules->region)
	      region = rules->region - rules->length;
	    else
	      region = fb->length - rules->length;

	    for (n = 0; n < region; n ++)
	      if ((result = (memcmp(fb->buffer + rules->offset - fb->offset + n, rules->value.stringv, (size_t)rules->length) == 0)) != 0)
		break;
          }
	  break;

      default :
          if (rules->child != NULL)
	    result = mime_check_rules(filename, fb, rules->child);
	  else
	    result = 0;
	  break;
    }

   /*
    * If the logic is inverted, invert the result...
    */

    if (rules->invert)
      result = !result;

   /*
    * OK, now if the current logic is OR and this result is true, the this
    * rule set is true.  If the current logic is AND and this result is false,
    * the the rule set is false...
    */

    DEBUG_printf(("5mime_check_rules: result of test %p (MIME_MAGIC_%s) is %d",
                  rules, debug_ops[rules->op], result));

    if ((result && logic == MIME_MAGIC_OR) ||
        (!result && logic == MIME_MAGIC_AND))
      return (result);

   /*
    * Otherwise the jury is still out on this one, so move to the next rule.
    */

    rules = rules->next;
  }

  return (result);
}


/*
 * 'mime_patmatch()' - Pattern matching.
 */

static int				/* O - 1 if match, 0 if no match */
mime_patmatch(const char *s,		/* I - String to match against */
              const char *pat)		/* I - Pattern to match against */
{
 /*
  * Range check the input...
  */

  if (s == NULL || pat == NULL)
    return (0);

 /*
  * Loop through the pattern and match strings, and stop if we come to a
  * point where the strings don't match or we find a complete match.
  */

  while (*s != '\0' && *pat != '\0')
  {
    if (*pat == '*')
    {
     /*
      * Wildcard - 0 or more characters...
      */

      pat ++;
      if (*pat == '\0')
        return (1);	/* Last pattern char is *, so everything matches... */

     /*
      * Test all remaining combinations until we get to the end of the string.
      */

      while (*s != '\0')
      {
        if (mime_patmatch(s, pat))
	  return (1);

	s ++;
      }
    }
    else if (*pat == '?')
    {
     /*
      * Wildcard - 1 character...
      */

      pat ++;
      s ++;
      continue;
    }
    else if (*pat == '[')
    {
     /*
      * Match a character from the input set [chars]...
      */

      pat ++;
      while (*pat != ']' && *pat != '\0')
        if (*s == *pat)
	  break;
	else
	  pat ++;

      if (*pat == ']' || *pat == '\0')
        return (0);

      while (*pat != ']' && *pat != '\0')
        pat ++;

      if (*pat == ']')
        pat ++;

      continue;
    }
    else if (*pat == '\\')
    {
     /*
      * Handle quoted characters...
      */

      pat ++;
    }

   /*
    * Stop if the pattern and string don't match...
    */

    if (*pat++ != *s++)
      return (0);
  }

 /*
  * Done parsing the pattern and string; return 1 if the last character
  * matches and 0 otherwise...
  */

  return (*s == *pat);
}


/*
 * End of "$Id: type.c 12578 2015-03-30 19:07:29Z msweet $".
 */
