/*
 * "$Id$"
 *
 *   Common PCL functions for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   pcl_set_media_size() - Set media size using the page size command.
 *   pjl_write()          - Write a PJL command string, performing
 *                          substitutions as needed.
 */

/*
 * Include necessary headers...
 */

#include "driver.h"
#include "pcl-common.h"
#include <math.h>


/*
 * 'pcl_set_media_size()' - Set media size using the page size command.
 */

void
pcl_set_media_size(ppd_file_t *ppd,	/* I - PPD file */
                   float      width,	/* I - Width of page */
                   float      length)	/* I - Length of page */
{
  (void)width;

  printf("\033&l0O");			/* Set portrait orientation */

  if (ppd->model_number & PCL_PAPER_SIZE)
    switch ((int)(length + 0.5f))
    {
      case 419 : /* Postcard */
          printf("\033&l71A");		/* Set page size */
	  break;

      case 540 : /* Monarch Envelope */
          printf("\033&l80A");		/* Set page size */
	  break;

      case 567 : /* Double Postcard */
          printf("\033&l72A");		/* Set page size */
	  break;

      case 595 : /* A5 */
          printf("\033&l25A");		/* Set page size */
	  break;

      case 612 : /* Statement */
          printf("\033&l5A");		/* Set page size */
	  break;

      case 624 : /* DL Envelope */
          printf("\033&l90A");		/* Set page size */
	  break;

      case 649 : /* C5 Envelope */
          printf("\033&l91A");		/* Set page size */
	  break;

      case 684 : /* COM-10 Envelope */
          printf("\033&l81A");		/* Set page size */
	  break;

      case 709 : /* B5 Envelope */
          printf("\033&l100A");		/* Set page size */
	  break;

      case 729 : /* B5 */
          printf("\033&l45A");		/* Set page size */
	  break;

      case 756 : /* Executive */
          printf("\033&l1A");		/* Set page size */
	  break;

      case 792 : /* Letter */
          printf("\033&l2A");		/* Set page size */
	  break;

      case 842 : /* A4 */
          printf("\033&l26A");		/* Set page size */
	  break;

      case 936 : /* Foolscap */
          printf("\033&l23A");		/* Set page size */
	  break;

      case 1008 : /* Legal */
          printf("\033&l3A");		/* Set page size */
	  break;

      case 1032 : /* B4 */
          printf("\033&l46A");		/* Set page size */
	  break;

      case 1191 : /* A3 */
          printf("\033&l27A");		/* Set page size */
	  break;

      case 1224 : /* Tabloid */
          printf("\033&l6A");		/* Set page size */
	  break;

      default :
          printf("\033&l101A");		/* Set page size */
	  printf("\033&l6D\033&k12H");	/* Set 6 LPI, 10 CPI */
	  printf("\033&l%.2fP", length / 12.0);
					/* Set page length */
	  printf("\033&l%.0fF", length / 12.0);
					/* Set text length to page */
	  break;
    }
  else
  {
    printf("\033&l6D\033&k12H");	/* Set 6 LPI, 10 CPI */
    printf("\033&l%.2fP", length / 12.0);
					/* Set page length */
    printf("\033&l%.0fF", length / 12.0);
					/* Set text length to page */
  }

  printf("\033&l0L");			/* Turn off perforation skip */
  printf("\033&l0E");			/* Reset top margin to 0 */
}


/*
 * 'pjl_write()' - Write a PJL command string, performing substitutions as needed.
 */

void
pjl_write(ppd_file_t    *ppd,		/* I - PPD file */
          const char    *format,	/* I - Format string */
          const char    *value,		/* I - Value for %s */
	  int           job_id,		/* I - Job ID */
          const char    *user,		/* I - Username */
	  const char    *title,		/* I - Title */
	  int           num_options,	/* I - Number of options */
          cups_option_t *options)	/* I - Options */
{
  const char	*optval;		/* Option value */
  char		match[255],		/* Match string */
		*mptr;			/* Pointer into match string */


  if (!format)
    return;

  while (*format)
  {
    if (*format == '%')
    {
     /*
      * Perform substitution...
      */

      format ++;
      switch (*format)
      {
        case 'b' :			/* job-billing */
	    if ((optval = cupsGetOption("job-billing", num_options,
	                                options)) != NULL)
	      fputs(optval, stdout);
	    break;

	case 'h' :			/* job-originating-host-name */
	    if ((optval = cupsGetOption("job-originating-host-name",
	                                num_options, options)) != NULL)
	      fputs(optval, stdout);
	    break;

	case 'j' :			/* job-id */
	    printf("%d", job_id);
	    break;

	case 'n' :			/* CR + LF */
	    putchar('\r');
	    putchar('\n');
	    break;

	case 'q' :			/* double quote (") */
	    putchar('\"');
	    break;

	case 's' :			/* "value" */
	    if (value)
	      fputs(value, stdout);
	    break;

	case 't' :			/* job-name */
            fputs(title, stdout);
	    break;

	case 'u' :			/* job-originating-user-name */
            fputs(user, stdout);
	    break;

        case '?' :			/* ?value:string; */
           /*
	    * Get the match value...
	    */

	    for (format ++, mptr = match; *format && *format != ':'; format ++)
	      if (mptr < (match + sizeof(match) - 1))
	        *mptr++ = *format;

            if (!*format)
	      return;

           /*
	    * See if we have a match...
	    */

            format ++;
            *mptr = '\0';

	    if (!value || strcmp(match, value))
	    {
	     /*
	      * Value doesn't match; skip the string that follows...
	      */

              while (*format && *format != ';')
	        format ++;
	    }
	    else
	    {
	     /*
	      * Value matches; copy the string that follows...
	      */

              while (*format && *format != ';')
	        putchar(*format++);
	    }

	    if (!*format)
	      return;
	    break;

	default :			/* Anything else */
	    putchar('%');
	case '%' :			/* %% = single % */
	    putchar(*format);
	    break;
      }
    }
    else
      putchar(*format);

    format ++;
  }
}


/*
 * End of "$Id$".
 */
