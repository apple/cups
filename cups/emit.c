/*
 * "$Id: emit.c,v 1.13 1999/09/13 20:05:47 mike Exp $"
 *
 *   PPD code emission routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 * Contents:
 *
 *   ppdEmit()     - Emit code for marked options to a file.
 *   ppdEmitFd()   - Emit code for marked options to a file.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include <stdlib.h>
#include "string.h"

#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


/*
 * Local functions...
 */

static int	ppd_sort(ppd_choice_t **c1, ppd_choice_t **c2);
static int	ppd_collect(ppd_file_t *ppd, ppd_section_t section,
		            ppd_choice_t ***choices);


/*
 * 'ppdEmit()' - Emit code for marked options to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmit(ppd_file_t    *ppd,		/* I - PPD file record */
        FILE          *fp,		/* I - File to write to */
        ppd_section_t section)		/* I - Section to write */
{
  int		i,			/* Looping var */
		count;			/* Number of choices */
  ppd_choice_t	**choices;		/* Choices */
  ppd_size_t	*size;			/* Custom page size */


  if ((count = ppd_collect(ppd, section, &choices)) == 0)
    return (0);

  for (i = 0; i < count; i ++)
    if (section != PPD_ORDER_EXIT && section != PPD_ORDER_JCL)
    {
     /*
      * Send DSC comments with option...
      */

      if (fprintf(fp, "%%%%BeginFeature: %s %s\n",
                  ((ppd_option_t *)choices[i]->option)->keyword,
		  choices[i]->choice) < 0)
      {
        free(choices);
        return (-1);
      }

      if (strcmp(((ppd_option_t *)choices[i]->option)->keyword, "PageSize") == 0 &&
          strcmp(choices[i]->choice, "Custom") == 0)
      {
       /*
        * Variable size; write out standard size options (this should
	* eventually be changed to use the parameter positions defined
	* in the PPD file...)
	*/

        size = ppdPageSize(ppd, "Custom");
        fprintf(fp, "%.0f %.0f 0 0 0\n", size->width, size->length);

	if (choices[i]->code == NULL)
	{
	 /*
	  * This can happen with certain buggy PPD files that don't include
	  * a CustomPageSize command sequence...  We just use a generic
	  * Level 2 command sequence...
	  */

	  fputs("pop pop pop\n", fp);
	  fputs("<</PageSize[5 -2 roll]/ImagingBBox null>>setpagedevice\n", fp);
	}
      }

      if (choices[i]->code != NULL && choices[i]->code[0] != '\0')
      {
        if (fputs(choices[i]->code, fp) < 0)
        {
          free(choices);
          return (-1);
        }

        if (choices[i]->code[strlen(choices[i]->code) - 1] != '\n')
          putc('\n', fp);
      }

      if (fputs("%%EndFeature\n", fp) < 0)
      {
        free(choices);
        return (-1);
      }
    }
    else if (fputs(choices[i]->code, fp) < 0)
    {
      free(choices);
      return (-1);
    }

  free(choices);
  return (0);
}


/*
 * 'ppdEmitFd()' - Emit code for marked options to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmitFd(ppd_file_t    *ppd,		/* I - PPD file record */
          int           fd,		/* I - File to write to */
          ppd_section_t section)	/* I - Section to write */
{
  int		i,			/* Looping var */
		count;			/* Number of choices */
  ppd_choice_t	**choices;		/* Choices */
  char		buf[1024];		/* Output buffer for feature */


  if ((count = ppd_collect(ppd, section, &choices)) == 0)
    return (0);

  for (i = 0; i < count; i ++)
    if (section != PPD_ORDER_EXIT && section != PPD_ORDER_JCL)
    {
     /*
      * Send DSC comments with option...
      */

      sprintf(buf, "%%%%BeginFeature: %s %s\n",
              ((ppd_option_t *)choices[i]->option)->keyword, choices[i]->choice);

      if (write(fd, buf, strlen(buf)) < 1)
      {
        free(choices);
        return (-1);
      }

      if (write(fd, choices[i]->code, strlen(choices[i]->code)) < 1)
      {
        free(choices);
        return (-1);
      }

      if (write(fd, "%%EndFeature\n", 13) < 1)
      {
        free(choices);
        return (-1);
      }
    }
    else if (write(fd, choices[i]->code, strlen(choices[i]->code)) < 1)
    {
      free(choices);
      return (-1);
    }

  free(choices);
  return (0);
}


/*
 * 'ppd_sort()' - Sort options by ordering numbers...
 */

static int			/* O - -1 if c1 < c2, 0 if equal, 1 otherwise */
ppd_sort(ppd_choice_t **c1,	/* I - First choice */
         ppd_choice_t **c2)	/* I - Second choice */
{
  if (((ppd_option_t *)(*c1)->option)->order < ((ppd_option_t *)(*c2)->option)->order)
    return (-1);
  else if (((ppd_option_t *)(*c1)->option)->order > ((ppd_option_t *)(*c2)->option)->order)
    return (1);
  else
    return (0);
}


/*
 * 'ppd_collect()' - Collect all marked options that reside in the specified
 *                   section.
 */

static int				/* O - Number of options marked */
ppd_collect(ppd_file_t    *ppd,		/* I - PPD file data */
            ppd_section_t section,	/* I - Section to collect */
            ppd_choice_t  ***choices)	/* O - Pointers to choices */
{
  int		i, j, k, m;		/* Looping vars */
  ppd_group_t	*g,			/* Current group */
		*sg;			/* Current sub-group */
  ppd_option_t	*o;			/* Current option */
  ppd_choice_t	*c;			/* Current choice */
  int		count;			/* Number of choices collected */
  ppd_choice_t	**collect;		/* Collected choices */


  if (ppd == NULL)
    return (0);

 /*
  * Allocate memory for up to 1000 selected choices...
  */

  count   = 0;
  collect = calloc(sizeof(ppd_choice_t *), 1000);

 /*
  * Loop through all options and add choices as needed...
  */

  for (i = ppd->num_groups, g = ppd->groups; i > 0; i --, g ++)
  {
    for (j = g->num_options, o = g->options; j > 0; j --, o ++)
      if (o->section == section)
	for (k = o->num_choices, c = o->choices; k > 0; k --, c ++)
	  if (c->marked && count < 1000)
	  {
            collect[count] = c;
	    count ++;
	  }

    for (j = g->num_subgroups, sg = g->subgroups; j > 0; j --, sg ++)
      for (k = sg->num_options, o = sg->options; k > 0; k --, o ++)
	if (o->section == section)
	  for (m = o->num_choices, c = o->choices; m > 0; m --, c ++)
	    if (c->marked && count < 1000)
	    {
              collect[count] = c;
	      count ++;
	    }
  }

 /*
  * If we have more than 1 marked choice, sort them...
  */

  if (count > 1)
    qsort(collect, count, sizeof(ppd_choice_t *),
          (int (*)(const void *, const void *))ppd_sort);

 /*
  * Return the array and number of choices; if 0, free the array since
  * it isn't needed.
  */

  if (count > 0)
  {
    *choices = collect;
    return (count);
  }
  else
  {
    *choices = NULL;
    free(collect);
    return (0);
  }
}


/*
 * End of "$Id: emit.c,v 1.13 1999/09/13 20:05:47 mike Exp $".
 */
