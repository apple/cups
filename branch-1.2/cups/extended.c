/*
 * "$Id$"
 *
 *   Extended option routines for the Common UNIX Printing System (CUPS).
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
 *   ppdFindExtOption()    - Return a pointer to the extended option.
 *   ppdMarkCurve()        - Mark an extended curve option.
 *   ppdMarkGamma()        - Mark an extended gamma option.
 *   ppdMarkInteger()      - Mark an extended integer option.
 *   ppdMarkIntegerArray() - Mark an extended integer array option.
 *   ppdMarkReal()         - Mark an extended real option.
 *   ppdMarkRealArray()    - Mark an extended real array option.
 *   ppdMarkText()         - Mark an extended text option.
 */

/*
 * Include necessary headers...
 */

#include "ppd.h"
#include "string.h"
#include "debug.h"


/*
 * Local functions...
 */

static void	ppd_unmark_choices(ppd_option_t *option);


/*
 * 'ppdFindExtOption()' - Return a pointer to the extended option.
 */

ppd_ext_option_t *			/* O - Pointer to option or NULL */
ppdFindExtOption(ppd_file_t *ppd,	/* I - PPD file data */
                 const char *option)	/* I - Option/Keyword name */
{
  int			i;		/* Looping var */
  ppd_ext_option_t	**o;		/* Pointer to option */


  if (ppd == NULL || option == NULL)
    return (NULL);

  for (i = ppd->num_extended, o = ppd->extended; i > 0; i --, o ++)
    if (strcasecmp(o[0]->keyword, option) == 0)
      return (*o);

  return (NULL);
}


/*
 * 'ppdFindExtParam()' - Find an extended parameter.
 */

ppd_ext_param_t *			/* O - Parameter or NULL */
ppdFindExtParam(ppd_ext_option_t *opt,	/* I - Option */
                const char       *param)/* I - Parameter name */
{
  int			i;		/* Looping var */
  ppd_ext_param_t	**p;		/* Pointer to parameter */


  if (opt == NULL || param == NULL)
    return (NULL);

  for (i = opt->num_params, p = opt->params; i > 0; i --, p ++)
    if (strcasecmp(p[0]->keyword, param) == 0)
      return (*p);

  return (NULL);
}


/*
 * 'ppdMarkCurve()' - Mark an extended curve option.
 */

int					/* O - Number of conflicts */
ppdMarkCurve(ppd_file_t *ppd,		/* I - PPD file */
             const char *keyword,	/* I - Option name */
             const char *param,		/* I - Parameter name */
             float      low,		/* I - Lower (start) value */
	     float      high,		/* I - Upper (end) value */
	     float      gvalue)		/* I - Gamma value for range */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkGamma()' - Mark an extended gamma option.
 */

int					/* O - Number of conflicts */
ppdMarkGamma(ppd_file_t *ppd,		/* I - PPD file */
             const char *keyword,	/* I - Option name */
             const char *param,		/* I - Parameter name */
             float      gvalue)		/* I - Gamma value */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkInteger()' - Mark an extended integer option.
 */

int					/* O - Number of conflicts */
ppdMarkInteger(ppd_file_t *ppd,		/* I - PPD file */
               const char *keyword,	/* I - Option name */
               const char *param,	/* I - Parameter name */
               int        value)	/* I - Option value */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkIntegerArray()' - Mark an extended integer array option.
 */

int					/* O - Number of conflicts */
ppdMarkIntegerArray(ppd_file_t *ppd,	/* I - PPD file */
                    const char *keyword,/* I - Option name */
	            const char *param,	/* I - Parameter name */
                    int        num_values,
		    			/* I - Number of values */
		    const int  *values)	/* I - Values */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkReal()' - Mark an extended real option.
 */

int					/* O - Number of conflicts */
ppdMarkReal(ppd_file_t *ppd,		/* I - PPD file */
            const char *keyword,	/* I - Option name */
            const char *param,		/* I - Parameter name */
            float      value)		/* I - Option value */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkRealArray()' - Mark an extended real array option.
 */

int					/* O - Number of conflicts */
ppdMarkRealArray(ppd_file_t  *ppd,	/* I - PPD file */
                 const char  *keyword,	/* I - Option name */
                 const char  *param,	/* I - Parameter name */
                 int         num_values,/* I - Number of values */
		 const float *values)	/* I - Values */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppdMarkText()' - Mark an extended text option.
 */

int					/* O - Number of conflicts */
ppdMarkText(ppd_file_t *ppd,		/* I - PPD file */
            const char *keyword,	/* I - Option name */
            const char *param,		/* I - Parameter name */
            const char *value)		/* I - Option value */
{
  ppd_ext_option_t	*o;		/* Extended option */
  ppd_ext_param_t	*p;		/* Extended parameter */


  if ((o = ppdFindExtOption(ppd, keyword)) == NULL)
    return (-1);

  if ((p = ppdFindExtParam(o, param)) == NULL)
    return (-1);

  ppd_unmark_choices(o->option);

  return (ppdConflicts(ppd));
}


/*
 * 'ppd_unmark_choices()' - Unmark all "canned" choices.
 */

static void
ppd_unmark_choices(ppd_option_t *option)/* I - Option choice */
{
  int		i;			/* Looping var */
  ppd_choice_t	*c;			/* Current choice */


  for (i = option->num_choices, c = option->choices; i > 0; i --, c++)
    c->marked = 0;
}


/*
 * End of "$Id$".
 */
