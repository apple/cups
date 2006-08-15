/*
 * "$Id$"
 *
 *   PPD custom option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2006 by Easy Software Products, all rights reserved.
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   ppdLocalize() - Localize the PPD file to the current locale.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "debug.h"


/*
 * Local functions...
 */

static const char	*ppd_text(ppd_file_t *ppd, const char *keyword,
			          const char *spec, const char *ll_CC,
				  const char *ll);


/*
 * 'ppdLocalize()' - Localize the PPD file to the current locale.
 *
 * @since CUPS 1.2@
 */

int					/* O - 0 on success, -1 on error */
ppdLocalize(ppd_file_t *ppd)		/* I - PPD file */
{
  int		i, j, k;		/* Looping vars */
  ppd_group_t	*group;			/* Current group */
  ppd_option_t	*option;		/* Current option */
  ppd_choice_t	*choice;		/* Current choice */
  ppd_coption_t	*coption;		/* Current custom option */
  ppd_cparam_t	*cparam;		/* Current custom parameter */
  cups_lang_t	*lang;			/* Current language */
  char		ckeyword[PPD_MAX_NAME],	/* Custom keyword */
		ll_CC[6],		/* Language + country locale */
		ll[3];			/* Language locale */
  const char	*text;			/* Localized text */


 /*
  * Range check input...
  */

  DEBUG_printf(("ppdLocalize(ppd=%p)\n", ppd));

  if (!ppd)
    return (-1);

 /*
  * Get the default language...
  */

  if ((lang = cupsLangDefault()) == NULL)
    return (-1);

  strlcpy(ll_CC, lang->language, sizeof(ll_CC));
  strlcpy(ll, lang->language, sizeof(ll));

  DEBUG_printf(("    lang->language=\"%s\", ll=\"%s\", ll_CC=\"%s\"...\n",
                lang->language, ll, ll_CC));

 /*
  * Now lookup all of the groups, options, choices, etc.
  */

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
  {
    if ((text = ppd_text(ppd, "Translation", group->name, ll_CC, ll)) != NULL)
      strlcpy(group->text, text, sizeof(group->text));

    for (j = group->num_options, option = group->options; j > 0; j --, option ++)
    {
      if ((text = ppd_text(ppd, "Translation", option->keyword, ll_CC,
                           ll)) != NULL)
	strlcpy(option->text, text, sizeof(option->text));

      for (k = option->num_choices, choice = option->choices;
           k > 0;
	   k --, choice ++)
      {
        if (strcmp(choice->choice, "Custom"))
	  text = ppd_text(ppd, option->keyword, choice->choice, ll_CC, ll);
	else
	{
	  snprintf(ckeyword, sizeof(ckeyword), "Custom%s", option->keyword);

	  text = ppd_text(ppd, ckeyword, "True", ll_CC, ll);
	}

        if (text)
	  strlcpy(choice->text, text, sizeof(choice->text));
      }
    }
  }

 /*
  * Translate any custom parameters...
  */

  for (coption = (ppd_coption_t *)cupsArrayFirst(ppd->coptions);
       coption;
       coption = (ppd_coption_t *)cupsArrayNext(ppd->coptions))
  {
    for (cparam = (ppd_cparam_t *)cupsArrayFirst(coption->params);
	 cparam;
	 cparam = (ppd_cparam_t *)cupsArrayNext(coption->params))
    {
      snprintf(ckeyword, sizeof(ckeyword), "ParamCustom%s", coption->keyword);

      if ((text = ppd_text(ppd, ckeyword, cparam->name, ll_CC, ll)) != NULL)
        strlcpy(cparam->text, text, sizeof(cparam->text));
    }
  }

  return (0);
}


/*
 * 'ppd_text()' - Find the localized text as needed...
 */

static const char *			/* O - Localized text or NULL */
ppd_text(ppd_file_t *ppd,		/* I - PPD file */
         const char *keyword,		/* I - Main keyword */
         const char *spec,		/* I - Option keyword */
	 const char *ll_CC,		/* I - Language + country locale */
	 const char *ll)		/* I - Language locale */
{
  char		lkeyword[PPD_MAX_NAME];	/* Localization keyword */
  ppd_attr_t	*attr;			/* Current attribute */


  DEBUG_printf(("ppd_text(ppd=%p, keyword=\"%s\", spec=\"%s\", "
                "ll_CC=\"%s\", ll=\"%s\")\n",
                ppd, keyword, spec, ll_CC, ll));

 /*
  * Look for Keyword.ll_CC, then Keyword.ll...
  */

  snprintf(lkeyword, sizeof(lkeyword), "%s.%s", ll_CC, keyword);
  if ((attr = ppdFindAttr(ppd, lkeyword, spec)) == NULL)
  {
    snprintf(lkeyword, sizeof(lkeyword), "%s.%s", ll, keyword);
    attr = ppdFindAttr(ppd, lkeyword, spec);
  }

#ifdef DEBUG
  if (attr)
    printf("    *%s %s/%s: \"%s\"\n", attr->name, attr->spec, attr->text,
           attr->value ? attr->value : "");
  else
    puts("    NOT FOUND");
#endif /* DEBUG */

 /*
  * Return text if we find it...
  */

  return (attr ? attr->text : NULL);
}


/*
 * End of "$Id$".
 */
