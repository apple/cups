/*
 * "$Id: snprintf.c,v 1.4.2.7 2003/01/07 18:26:29 mike Exp $"
 *
 *   snprintf functions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cups_vsnprintf() - Format a string into a fixed size buffer.
 *   cups_snprintf()  - Format a string into a fixed size buffer.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <ctype.h>
#include "string.h"


#ifndef HAVE_VSNPRINTF
/*
 * 'cups_vsnprintf()' - Format a string into a fixed size buffer.
 */

int					/* O - Number of bytes formatted */
cups_vsnprintf(char       *buffer,	/* O - Output buffer */
              size_t     bufsize,	/* O - Size of output buffer */
	      const char *format,	/* I - printf-style format string */
	      va_list    ap)		/* I - Pointer to additional arguments */
{
  char		*bufptr,		/* Pointer to position in buffer */
		*bufend,		/* Pointer to end of buffer */
		sign,			/* Sign of format width */
		size,			/* Size character (h, l, L) */
		type;			/* Format type character */
  const char	*bufformat;		/* Start of format */
  int		width,			/* Width of field */
		prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for sprintf() */
		temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */
  int		slen;			/* Length of string */


 /*
  * Loop through the format string, formatting as needed...
  */

  bufptr = buffer;
  bufend = buffer + bufsize - 1;

  while (*format && bufptr < bufend)
  {
    if (*format == '%')
    {
      bufformat = format;
      format ++;

      if (*format == '%')
      {
        *bufptr++ = *format++;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        sign = *format++;
      else
        sign = 0;

      width = 0;
      while (isdigit(*format))
        width = width * 10 + *format++ - '0';

      if (*format == '.')
      {
        format ++;
	prec = 0;

	while (isdigit(*format))
          prec = prec * 10 + *format++ - '0';
      }
      else
        prec = -1;

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';
	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
        size = *format++;

      if (!*format)
        break;

      type = *format++;

      switch (type)
      {
	case 'E' : /* Floating point formats */
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((format - bufformat + 1) > sizeof(tformat) ||
	        (width + 2) > sizeof(temp))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    sprintf(temp, tformat, va_arg(ap, double));

	    if ((bufptr + strlen(temp)) > bufend)
	    {
	      strncpy(bufptr, temp, bufend - bufptr);
	      bufptr = bufend;
	      break;
	    }
	    else
	    {
	      strcpy(bufptr, temp);
	      bufptr += strlen(temp);
	    }
	    break;

        case 'B' : /* Integer formats */
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((format - bufformat + 1) > sizeof(tformat) ||
	        (width + 2) > sizeof(temp))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    sprintf(temp, tformat, va_arg(ap, int));

	    if ((bufptr + strlen(temp)) > bufend)
	    {
	      strncpy(bufptr, temp, bufend - bufptr);
	      bufptr = bufend;
	      break;
	    }
	    else
	    {
	      strcpy(bufptr, temp);
	      bufptr += strlen(temp);
	    }
	    break;
	    
	case 'p' : /* Pointer value */
	    if ((format - bufformat + 1) > sizeof(tformat) ||
	        (width + 2) > sizeof(temp))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    sprintf(temp, tformat, va_arg(ap, void *));

	    if ((bufptr + strlen(temp)) > bufend)
	    {
	      strncpy(bufptr, temp, bufend - bufptr);
	      bufptr = bufend;
	      break;
	    }
	    else
	    {
	      strcpy(bufptr, temp);
	      bufptr += strlen(temp);
	    }
	    break;

        case 'c' : /* Character or character array */
	    if (width <= 1)
	      *bufptr++ = va_arg(ap, int);
	    else
	    {
	      if ((bufptr + width) > bufend)
	        width = bufend - bufptr;

	      memcpy(bufptr, va_arg(ap, char *), width);
	      bufptr += width;
	    }
	    break;

	case 's' : /* String */
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = "(null)";

	    slen = strlen(s);
	    if (slen > width && prec != width)
	      width = slen;

	    if ((bufptr + width) > bufend)
	      width = bufend - bufptr;

            if (slen > width)
	      slen = width;

	    if (sign == '-')
	    {
	      strncpy(bufptr, s, slen);
	      memset(bufptr + slen, ' ', width - slen);
	    }
	    else
	    {
	      memset(bufptr, ' ', width - slen);
	      strncpy(bufptr + width - slen, s, slen);
	    }

	    bufptr += width;
	    break;

	case 'n' : /* Output number of chars so far */
	    if ((format - bufformat + 1) > sizeof(tformat) ||
	        (width + 2) > sizeof(temp))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    sprintf(temp, tformat, va_arg(ap, int));

	    if ((bufptr + strlen(temp)) > bufend)
	    {
	      strncpy(bufptr, temp, bufend - bufptr);
	      bufptr = bufend;
	      break;
	    }
	    else
	    {
	      strcpy(bufptr, temp);
	      bufptr += strlen(temp);
	    }
	    break;
      }
    }
    else
      *bufptr++ = *format++;
  }

 /*
  * Nul-terminate the string and return the number of characters in it.
  */

  *bufptr = '\0';
  return (bufptr - buffer);
}
#endif /* !HAVE_VSNPRINT */


#ifndef HAVE_SNPRINTF
/*
 * 'cups_snprintf()' - Format a string into a fixed size buffer.
 */

int					/* O - Number of bytes formatted */
cups_snprintf(char       *buffer,	/* O - Output buffer */
             size_t     bufsize,	/* O - Size of output buffer */
             const char *format,	/* I - printf-style format string */
	     ...)			/* I - Additional arguments as needed */
{
  int		bytes;			/* Number of bytes formatted */
  va_list 	ap;			/* Pointer to additional arguments */


  va_start(ap, format);
  bytes = vsnprintf(buffer, bufsize, format, ap);
  va_end(ap);

  return (bytes);
}
#endif /* !HAVE_SNPRINTF */


/*
 * End of "$Id: snprintf.c,v 1.4.2.7 2003/01/07 18:26:29 mike Exp $".
 */

