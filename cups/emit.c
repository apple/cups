/*
 * "$Id$"
 *
 *   PPD code emission routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 *   PostScript is a trademark of Adobe Systems, Inc.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdCollect()       - Collect all marked options that reside in the
 *                        specified section.
 *   ppdEmit()          - Emit code for marked options to a file.
 *   ppdEmitFd()        - Emit code for marked options to a file.
 *   ppdEmitJCL()       - Emit code for JCL options to a file.
 *   ppdEmitJCLEnd()    - Emit JCLEnd code to a file.
 *   ppd_handle_media() - Handle media selection...
 *   ppd_sort()         - Sort options by ordering numbers...
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

static void	ppd_handle_media(ppd_file_t *ppd);
static int	ppd_sort(ppd_choice_t **c1, ppd_choice_t **c2);


/*
 * Local globals...
 */

static const char ppd_custom_code[] =
		"pop pop pop\n"
		"<</PageSize[5 -2 roll]/ImagingBBox null>>setpagedevice\n";


/*
 * 'ppdCollect()' - Collect all marked options that reside in the specified
 *                  section.
 */

int					/* O - Number of options marked */
ppdCollect(ppd_file_t    *ppd,		/* I - PPD file data */
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


 /*
  * Use PageSize or PageRegion as required...
  */

  ppd_handle_media(ppd);

 /*
  * Collect the options we need to emit and emit them!
  */

  if ((count = ppdCollect(ppd, section, &choices)) == 0)
    return (0);

  for (i = 0; i < count; i ++)
    if (section != PPD_ORDER_EXIT && section != PPD_ORDER_JCL)
    {
     /*
      * Send wrapper commands to prevent printer errors for unsupported
      * options...
      */

      if (fputs("[{\n", fp) < 0)
      {
        free(choices);
        return (-1);
      }

     /*
      * Send DSC comments with option...
      */

      if ((strcasecmp(((ppd_option_t *)choices[i]->option)->keyword, "PageSize") == 0 ||
           strcasecmp(((ppd_option_t *)choices[i]->option)->keyword, "PageRegion") == 0) &&
          strcasecmp(choices[i]->choice, "Custom") == 0)
      {
       /*
        * Variable size; write out standard size options, using the
	* parameter positions defined in the PPD file...
	*/

        ppd_attr_t	*attr;		/* PPD attribute */
	int		pos,		/* Position of custom value */
			values[5],	/* Values for custom command */
			orientation;	/* Orientation to use */


        fputs("%%BeginFeature: *CustomPageSize True\n", fp);

        size = ppdPageSize(ppd, "Custom");

        memset(values, 0, sizeof(values));

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize", "Width")) != NULL)
	{
	  pos = atoi(attr->value) - 1;

          if (pos < 0 || pos > 4)
	    pos = 0;
	}
	else
	  pos = 0;

	values[pos] = (int)size->width;

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize", "Height")) != NULL)
	{
	  pos = atoi(attr->value) - 1;

          if (pos < 0 || pos > 4)
	    pos = 1;
	}
	else
	  pos = 1;

	values[pos] = (int)size->length;

       /*
        * According to the Adobe PPD specification, an orientation of 1
	* will produce a print that comes out upside-down with the X
	* axis perpendicular to the direction of feed, which is exactly
	* what we want to be consistent with non-PS printers.
	*
	* We could also use an orientation of 3 to produce output that
	* comes out rightside-up (this is the default for many large format
	* printer PPDs), however for consistency we will stick with the
	* value 1.
	*
	* If we wanted to get fancy, we could use orientations of 0 or
	* 2 and swap the width and length, however we don't want to get
	* fancy, we just want it to work consistently.
	*
	* The orientation value is range limited by the Orientation
	* parameter definition, so certain non-PS printer drivers that
	* only support an Orientation of 0 will get the value 0 as
	* expected.
	*/

        orientation = 1;

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize",
	                        "Orientation")) != NULL)
	{
	  int min_orient, max_orient;	/* Minimum and maximum orientations */


          if (sscanf(attr->value, "%d%*s%d%d", &pos, &min_orient,
	             &max_orient) != 3)
	    pos = 4;
	  else
	  {
	    pos --;

            if (pos < 0 || pos > 4)
	      pos = 4;

            if (orientation > max_orient)
	      orientation = max_orient;
	    else if (orientation < min_orient)
	      orientation = min_orient;
	  }
	}
	else
	  pos = 4;

	values[pos] = orientation;

        fprintf(fp, "%d %d %d %d %d\n", values[0], values[1],
	        values[2], values[3], values[4]);

	if (choices[i]->code == NULL)
	{
	 /*
	  * This can happen with certain buggy PPD files that don't include
	  * a CustomPageSize command sequence...  We just use a generic
	  * Level 2 command sequence...
	  */

	  fputs(ppd_custom_code, fp);
	}
      }
      else if (fprintf(fp, "%%%%BeginFeature: *%s %s\n",
                       ((ppd_option_t *)choices[i]->option)->keyword,
		       choices[i]->choice) < 0)
      {
        free(choices);
        return (-1);
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

      if (fputs("} stopped cleartomark\n", fp) < 0)
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
		count,			/* Number of choices */
		custom_size;		/* Non-zero if this option is a custom size */
  ppd_choice_t	**choices;		/* Choices */
  ppd_size_t	*size;			/* Custom page size */
  char		buf[1024];		/* Output buffer for feature */


 /*
  * Use PageSize or PageRegion as required...
  */

  ppd_handle_media(ppd);

 /*
  * Collect the options we need to emit and emit them!
  */

  if ((count = ppdCollect(ppd, section, &choices)) == 0)
    return (0);

  for (i = 0; i < count; i ++)
    if (section != PPD_ORDER_EXIT && section != PPD_ORDER_JCL)
    {
     /*
      * Send wrapper commands to prevent printer errors for unsupported
      * options...
      */

      if (write(fd, "[{\n", 3) < 1)
      {
        free(choices);
        return (-1);
      }

     /*
      * Send DSC comments with option...
      */

      if ((strcasecmp(((ppd_option_t *)choices[i]->option)->keyword, "PageSize") == 0 ||
           strcasecmp(((ppd_option_t *)choices[i]->option)->keyword, "PageRegion") == 0) &&
          strcasecmp(choices[i]->choice, "Custom") == 0)
      {
        custom_size = 1;

	strcpy(buf, "%%BeginFeature: *CustomPageSize True\n");
      }
      else
      {
        custom_size = 0;

	snprintf(buf, sizeof(buf), "%%%%BeginFeature: *%s %s\n",
        	 ((ppd_option_t *)choices[i]->option)->keyword,
		 choices[i]->choice);
      }

      if (write(fd, buf, strlen(buf)) < 1)
      {
        free(choices);
        return (-1);
      }

      if (custom_size)
      {
       /*
        * Variable size; write out standard size options, using the
	* parameter positions defined in the PPD file...
	*/

        ppd_attr_t	*attr;		/* PPD attribute */
	int		pos,		/* Position of custom value */
			values[5],	/* Values for custom command */
			orientation;	/* Orientation to use */


        size = ppdPageSize(ppd, "Custom");

        memset(values, 0, sizeof(values));

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize", "Width")) != NULL)
	{
	  pos = atoi(attr->value) - 1;

          if (pos < 0 || pos > 4)
	    pos = 0;
	}
	else
	  pos = 0;

	values[pos] = (int)size->width;

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize", "Height")) != NULL)
	{
	  pos = atoi(attr->value) - 1;

          if (pos < 0 || pos > 4)
	    pos = 1;
	}
	else
	  pos = 1;

	values[pos] = (int)size->length;

        if (size->width < size->length)
	  orientation = 1;
	else
	  orientation = 0;

	if ((attr = ppdFindAttr(ppd, "ParamCustomPageSize",
	                        "Orientation")) != NULL)
	{
	  int min_orient, max_orient;	/* Minimum and maximum orientations */


          if (sscanf(attr->value, "%d%*s%d%d", &pos, &min_orient,
	             &max_orient) != 3)
	    pos = 4;
	  else
	  {
	    pos --;

            if (pos < 0 || pos > 4)
	      pos = 4;

            if (orientation > max_orient)
	      orientation = max_orient;
	    else if (orientation < min_orient)
	      orientation = min_orient;
	  }
	}
	else
	  pos = 4;

	values[pos] = orientation;

        snprintf(buf, sizeof(buf), "%d %d %d %d %d\n", values[0], values[1],
	         values[2], values[3], values[4]);

	if (write(fd, buf, strlen(buf)) < 1)
	{
          free(choices);
          return (-1);
	}

	if (choices[i]->code == NULL)
	{
	 /*
	  * This can happen with certain buggy PPD files that don't include
	  * a CustomPageSize command sequence...  We just use a generic
	  * Level 2 command sequence...
	  */

	  if (write(fd, ppd_custom_code, strlen(ppd_custom_code)) < 1)
	  {
            free(choices);
            return (-1);
	  }
	}
      }

      if (choices[i]->code != NULL && choices[i]->code[0] != '\0')
      {
	if (write(fd, choices[i]->code, strlen(choices[i]->code)) < 1)
	{
          free(choices);
          return (-1);
	}
      }

      if (write(fd, "%%EndFeature\n", 13) < 1)
      {
        free(choices);
        return (-1);
      }

      if (write(fd, "} stopped cleartomark\n", 22) < 1)
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
 * 'ppdEmitJCL()' - Emit code for JCL options to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmitJCL(ppd_file_t *ppd,		/* I - PPD file record */
           FILE       *fp,		/* I - File to write to */
           int        job_id,		/* I - Job ID */
	   const char *user,		/* I - Username */
	   const char *title)		/* I - Title */
{
  char		*ptr;			/* Pointer into JCL string */
  char		temp[81];		/* Local title string */


 /*
  * Range check the input...
  */

  if (ppd == NULL || ppd->jcl_begin == NULL || ppd->jcl_ps == NULL)
    return (0);

 /*
  * See if the printer supports HP PJL...
  */

  if (!strncmp(ppd->jcl_begin, "\033%-12345X@", 10))
  {
   /*
    * This printer uses HP PJL commands for output; filter the output
    * so that we only have a single "@PJL JOB" command in the header...
    *
    * To avoid bugs in the PJL implementation of certain vendors' products
    * (Xerox in particular), we add a dummy "@PJL" command at the beginning
    * of the PJL commands to initialize PJL processing.
    */

    fputs("\033%-12345X@PJL\n", fp);
    for (ptr = ppd->jcl_begin + 9; *ptr;)
      if (!strncmp(ptr, "@PJL JOB", 8))
      {
       /*
        * Skip job command...
	*/

        for (;*ptr; ptr ++)
	  if (*ptr == '\n')
	    break;

	if (*ptr)
	  ptr ++;
      }
      else
      {
       /*
        * Copy line...
	*/

        for (;*ptr; ptr ++)
	{
	  putc(*ptr, fp);
	  if (*ptr == '\n')
	    break;
	}

	if (*ptr)
	  ptr ++;
      }

   /*
    * Eliminate any path info from the job title...
    */

    if ((ptr = strrchr(title, '/')) != NULL)
      title = ptr + 1;

   /*
    * Replace double quotes with single quotes so that the title
    * does not cause a PJL syntax error.
    */

    strlcpy(temp, title, sizeof(temp));

    for (ptr = temp; *ptr; ptr ++)
      if (*ptr == '\"')
        *ptr = '\'';

   /*
    * Send PJL JOB and PJL RDYMSG commands before we enter PostScript mode...
    */

    fprintf(fp, "@PJL JOB NAME = \"%s\" DISPLAY = \"%d %s %s\"\n", temp,
            job_id, user, temp);
    fprintf(fp, "@PJL RDYMSG DISPLAY = \"%d %s %s\"\n", job_id, user, temp);
  }
  else
    fputs(ppd->jcl_begin, fp);

  ppdEmit(ppd, fp, PPD_ORDER_JCL);
  fputs(ppd->jcl_ps, fp);

  return (0);
}


/*
 * 'ppdEmitJCLEnd()' - Emit JCLEnd code to a file.
 */

int					/* O - 0 on success, -1 on failure */
ppdEmitJCLEnd(ppd_file_t *ppd,		/* I - PPD file record */
              FILE       *fp)		/* I - File to write to */
{
  ppd_attr_t	*attr;			/* PPD attributes */


 /*
  * Range check the input...
  */

  if (ppd == NULL)
    return (0);

  if (ppd->jcl_end == NULL)
  {
    if (ppd->num_filters == 0)
      fputc(0x04, fp);

    if ((attr = ppdFindAttr(ppd, "cupsProtocol", NULL)) != NULL &&
        attr->value != NULL && !strcasecmp(attr->value, "TBCP"))
      fputs("\033%-12345X", stdout);

    return (0);
  }

 /*
  * See if the printer supports HP PJL...
  */

  if (!strncmp(ppd->jcl_end, "\033%-12345X@", 10))
  {
   /*
    * This printer uses HP PJL commands for output; filter the output
    * so that we only have a single "@PJL JOB" command in the header...
    *
    * To avoid bugs in the PJL implementation of certain vendors' products
    * (Xerox in particular), we add a dummy "@PJL" command at the beginning
    * of the PJL commands to initialize PJL processing.
    */

    fputs("\033%-12345X@PJL\n", fp);
    fputs("@PJL RDYMSG DISPLAY = \"READY\"\n", fp);
    fputs(ppd->jcl_end + 9, fp);
  }
  else
    fputs(ppd->jcl_end, fp);

  return (0);
}


/*
 * 'ppd_handle_media()' - Handle media selection...
 */

static void
ppd_handle_media(ppd_file_t *ppd)
{
  ppd_choice_t	*manual_feed,		/* ManualFeed choice, if any */
		*input_slot,		/* InputSlot choice, if any */
		*page;			/* PageSize/PageRegion */
  ppd_size_t	*size;			/* Current media size */
  ppd_attr_t	*rpr;			/* RequiresPageRegion value */


 /*
  * This function determines if the user has selected a media source
  * via the InputSlot or ManualFeed options; if so, it marks the
  * PageRegion option corresponding to the current media size.
  * Otherwise it marks the PageSize option.
  */

  if ((size = ppdPageSize(ppd, NULL)) == NULL)
    return;

  manual_feed = ppdFindMarkedChoice(ppd, "ManualFeed");
  input_slot  = ppdFindMarkedChoice(ppd, "InputSlot");

  if (input_slot != NULL)
    rpr = ppdFindAttr(ppd, "RequiresPageRegion", input_slot->choice);
  else
    rpr = NULL;

  if (!rpr)
    rpr = ppdFindAttr(ppd, "RequiresPageRegion", "All");

  if (strcasecmp(size->name, "Custom") == 0 ||
      (manual_feed == NULL && input_slot == NULL) ||
      (manual_feed != NULL && strcasecmp(manual_feed->choice, "False") == 0) ||
      (input_slot != NULL && (input_slot->code == NULL || !input_slot->code[0])))
  {
   /*
    * Manual feed was not selected and/or the input slot selection does
    * not contain any PostScript code.  Use the PageSize option...
    */

    ppdMarkOption(ppd, "PageSize", size->name);
  }
  else
  {
   /*
    * Manual feed was selected and/or the input slot selection contains
    * PostScript code.  Use the PageRegion option...
    */

    ppdMarkOption(ppd, "PageRegion", size->name);

   /*
    * RequiresPageRegion does not apply to manual feed so we need to
    * check that we are not doing manual feed before unmarking PageRegion.
    */

    if (!(manual_feed && !strcasecmp(manual_feed->choice, "True")) &&
        ((rpr && rpr->value && !strcmp(rpr->value, "False")) ||
         (!rpr && !ppd->num_filters)))
    {
     /*
      * Either the PPD file specifies no PageRegion code or the PPD file
      * not for a CUPS raster driver and thus defaults to no PageRegion
      * code...  Unmark the PageRegion choice so that we don't output the
      * code...
      */

      page = ppdFindMarkedChoice(ppd, "PageRegion");

      if (page)
        page->marked = 0;
    }
  }
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
 * End of "$Id$".
 */
