/*
 * "$Id$"
 *
 *   PPD custom option routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
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
 *   ppdLocalize()          - Localize the PPD file to the current locale.
 *   ppdLocalizeIPPReason() - Get the localized version of a cupsIPPReason
 *                            attribute.
 *   ppd_ll_CC()            - Get the current locale names.
 *   ppd_localized_attr()   - Find a localized attribute.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "debug.h"


/*
 * Local functions...
 */

static void		ppd_ll_CC(char *ll_CC, int ll_CC_size,
			          char *ll, int ll_size);
static ppd_attr_t	*ppd_localized_attr(ppd_file_t *ppd,
			                    const char *keyword,
			                    const char *spec, const char *ll_CC,
				            const char *ll);


/*
 * 'ppdLocalize()' - Localize the PPD file to the current locale.
 *
 * All groups, options, and choices are localized, as are ICC profile
 * descriptions, printer presets, and custom option parameters.  Each
 * localized string uses the UTF-8 character encoding.
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
  ppd_attr_t	*attr,			/* Current attribute */
		*locattr;		/* Localized attribute */
  char		ckeyword[PPD_MAX_NAME],	/* Custom keyword */
		ll_CC[6],		/* Language + country locale */
		ll[3];			/* Language locale */


 /*
  * Range check input...
  */

  DEBUG_printf(("ppdLocalize(ppd=%p)\n", ppd));

  if (!ppd)
    return (-1);

 /*
  * Get the default language...
  */

  ppd_ll_CC(ll_CC, sizeof(ll_CC), ll, sizeof(ll));

 /*
  * Now lookup all of the groups, options, choices, etc.
  */

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
  {
    if ((locattr = ppd_localized_attr(ppd, "Translation", group->name,
                                      ll_CC, ll)) != NULL)
      strlcpy(group->text, locattr->text, sizeof(group->text));

    for (j = group->num_options, option = group->options; j > 0; j --, option ++)
    {
      if ((locattr = ppd_localized_attr(ppd, "Translation", option->keyword,
                                        ll_CC, ll)) != NULL)
	strlcpy(option->text, locattr->text, sizeof(option->text));

      for (k = option->num_choices, choice = option->choices;
           k > 0;
	   k --, choice ++)
      {
        if (strcmp(choice->choice, "Custom"))
	  locattr = ppd_localized_attr(ppd, option->keyword, choice->choice,
	                               ll_CC, ll);
	else
	{
	  snprintf(ckeyword, sizeof(ckeyword), "Custom%s", option->keyword);

	  locattr = ppd_localized_attr(ppd, ckeyword, "True", ll_CC, ll);
	}

        if (locattr)
	  strlcpy(choice->text, locattr->text, sizeof(choice->text));
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

      if ((locattr = ppd_localized_attr(ppd, ckeyword, cparam->name,
                                        ll_CC, ll)) != NULL)
        strlcpy(cparam->text, locattr->text, sizeof(cparam->text));
    }
  }

 /*
  * Translate ICC profile names...
  */

  if ((attr = ppdFindAttr(ppd, "APCustomColorMatchingName", NULL)) != NULL)
  {
    if ((locattr = ppd_localized_attr(ppd, "APCustomColorMatchingName",
                                      attr->spec, ll_CC, ll)) != NULL)
      strlcpy(attr->text, locattr->text, sizeof(attr->text));
  }

  for (attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
  {
    cupsArraySave(ppd->sorted_attrs);

    if ((locattr = ppd_localized_attr(ppd, "cupsICCProfile", attr->spec,
                                      ll_CC, ll)) != NULL)
      strlcpy(attr->text, locattr->text, sizeof(attr->text));

    cupsArrayRestore(ppd->sorted_attrs);
  }

 /*
  * Translate printer presets...
  */

  for (attr = ppdFindAttr(ppd, "APPrinterPreset", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "APPrinterPreset", NULL))
  {
    cupsArraySave(ppd->sorted_attrs);

    if ((locattr = ppd_localized_attr(ppd, "APPrinterPreset", attr->spec,
                                      ll_CC, ll)) != NULL)
      strlcpy(attr->text, locattr->text, sizeof(attr->text));

    cupsArrayRestore(ppd->sorted_attrs);
  }

  return (0);
}


/*
 * 'ppdLocalizeIPPReason()' - Get the localized version of a cupsIPPReason
 *                            attribute.
 *
 * This function uses the current locale to find the corresponding reason
 * text or URI from the attribute value. If "scheme" is NULL or "text",
 * the returned value contains human-readable (UTF-8) text from the translation
 * string or attribute value. Otherwise the corresponding URI is returned.
 *
 * If no value of the requested scheme can be found, NULL is returned.
 *
 * @since CUPS 1.3@
 */

const char *				/* O - Value or NULL if not found */
ppdLocalizeIPPReason(
    ppd_file_t *ppd,			/* I - PPD file */
    const char *reason,			/* I - IPP reason keyword to look up */
    const char *scheme,			/* I - URI scheme or NULL for text */
    char       *buffer,			/* I - Value buffer */
    size_t     bufsize)			/* I - Size of value buffer */
{
  ppd_attr_t	*locattr;		/* Localized attribute */
  char		ll_CC[6],		/* Language + country locale */
		ll[3],			/* Language locale */
		*bufptr,		/* Pointer into buffer */
		*bufend,		/* Pointer to end of buffer */
		*valptr;		/* Pointer into value */
  int		ch,			/* Hex-encoded character */
		schemelen;		/* Length of scheme name */


 /*
  * Range check input...
  */

  if (buffer)
    *buffer = '\0';

  if (!ppd || !reason || (scheme && !*scheme) ||
      !buffer || bufsize < PPD_MAX_TEXT)
    return (NULL);

 /*
  * Get the default language...
  */

  ppd_ll_CC(ll_CC, sizeof(ll_CC), ll, sizeof(ll));

 /*
  * Find the localized attribute...
  */

  if ((locattr = ppd_localized_attr(ppd, "cupsIPPReason", reason,
                                    ll_CC, ll)) == NULL)
    locattr = ppdFindAttr(ppd, "cupsIPPReason", reason);

  if (!locattr)
    return (NULL);

 /*
  * Now find the value we need...
  */

  bufend = buffer + bufsize - 1;

  if (!scheme || !strcmp(scheme, "text"))
  {
   /*
    * Copy a text value (either the translation text or text:... URIs from
    * the value...
    */

    strlcpy(buffer, locattr->text, bufsize);

    for (valptr = locattr->value, bufptr = buffer; *valptr && bufptr < bufend;)
    {
      if (!strncmp(valptr, "text:", 5))
      {
       /*
        * Decode text: URI and add to the buffer...
	*/

        if (bufptr > buffer)
	  *bufptr++ = ' ';		/* Add leading whitespace */

	valptr += 5;

        while (*valptr && !isspace(*valptr & 255) && bufptr < bufend)
	{
	  if (*valptr == '%' && isxdigit(valptr[1] & 255) &&
	      isxdigit(valptr[2] & 255))
	  {
	   /*
	    * Pull a hex-encoded character from the URI...
	    */

            valptr ++;

	    if (isdigit(*valptr & 255))
	      ch = (*valptr - '0') << 4;
	    else
	      ch = (tolower(*valptr) - 'a' + 10) << 4;
	    valptr ++;

	    if (isdigit(*valptr & 255))
	      *bufptr++ = ch | (*valptr - '0');
	    else
	      *bufptr++ = ch | (tolower(*valptr) - 'a' + 10);
	    valptr ++;
	  }
	  else if (*valptr == '+')
	  {
	    *bufptr++ = ' ';
	    valptr ++;
	  }
	  else
	    *bufptr++ = *valptr++;
        }
      }
      else
      {
       /*
        * Skip this URI...
	*/

        while (*valptr && !isspace(*valptr & 255))
          valptr++;
      }

     /*
      * Skip whitespace...
      */

      while (isspace(*valptr & 255))
	valptr ++;
    }

    if (bufptr > buffer)
      *bufptr = '\0';

    return (buffer);
  }
  else
  {
   /*
    * Copy a URI...
    */

    schemelen = strlen(scheme);
    if (scheme[schemelen - 1] == ':')	/* Force scheme to be just the name */
      schemelen --;

    for (valptr = locattr->value, bufptr = buffer; *valptr && bufptr < bufend;)
    {
      if ((!strncmp(valptr, scheme, schemelen) && valptr[schemelen] == ':') ||
          (*valptr == '/' && !strcmp(scheme, "file")))
      {
       /*
        * Copy URI...
	*/

        while (*valptr && !isspace(*valptr & 255) && bufptr < bufend)
	  *bufptr++ = *valptr++;

	*bufptr = '\0';

	return (buffer);
      }
      else
      {
       /*
        * Skip this URI...
	*/

	while (*valptr && !isspace(*valptr & 255))
	  valptr++;
      }

     /*
      * Skip whitespace...
      */

      while (isspace(*valptr & 255))
	valptr ++;
    }

    return (NULL);
  }
}


/*
 * 'ppd_ll_CC()' - Get the current locale names.
 */

static void
ppd_ll_CC(char *ll_CC,			/* O - Country-specific locale name */
          int  ll_CC_size,		/* I - Size of country-specific name */
          char *ll,			/* O - Generic locale name */
          int  ll_size)			/* I - Size of generic name */
{
  cups_lang_t	*lang;			/* Current language */


 /*
  * Get the current locale...
  */

  if ((lang = cupsLangDefault()) == NULL)
  {
    strlcpy(ll_CC, "en_US", ll_CC_size);
    strlcpy(ll, "en", ll_size);
    return;
  }

 /*
  * Copy the locale name...
  */

  strlcpy(ll_CC, lang->language, ll_CC_size);
  strlcpy(ll, lang->language, ll_size);

  DEBUG_printf(("ll_CC=\"%s\", ll=\"%s\"\n", ll_CC, ll));

  if (strlen(ll_CC) == 2)
  {
   /*
    * Map "ll" to primary/origin country locales to have the best
    * chance of finding a match...
    */

    if (!strcmp(ll_CC, "cs"))
      strlcpy(ll_CC, "cs_CZ", ll_CC_size);
    else if (!strcmp(ll_CC, "en"))
      strlcpy(ll_CC, "en_US", ll_CC_size);
    else if (!strcmp(ll_CC, "ja"))
      strlcpy(ll_CC, "ja_JP", ll_CC_size);
    else if (!strcmp(ll_CC, "sv"))
      strlcpy(ll_CC, "sv_SE", ll_CC_size);
    else if (!strcmp(ll_CC, "zh"))	/* Simplified Chinese */
      strlcpy(ll_CC, "zh_CN", ll_CC_size);
    else if (ll_CC_size >= 6)
    {
      ll_CC[2] = '_';
      ll_CC[3] = toupper(ll_CC[0] & 255);
      ll_CC[4] = toupper(ll_CC[1] & 255);
      ll_CC[5] = '\0';
    }
  }

  DEBUG_printf(("ppd_ll_CC: lang->language=\"%s\", ll=\"%s\", ll_CC=\"%s\"...\n",
                lang->language, ll, ll_CC));
}


/*
 * 'ppd_localized_attr()' - Find a localized attribute.
 */

static ppd_attr_t *			/* O - Localized attribute or NULL */
ppd_localized_attr(ppd_file_t *ppd,	/* I - PPD file */
		   const char *keyword,	/* I - Main keyword */
		   const char *spec,	/* I - Option keyword */
		   const char *ll_CC,	/* I - Language + country locale */
		   const char *ll)	/* I - Language locale */
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

    if (!attr)
    {
      if (!strcmp(ll, "ja"))
      {
       /*
	* Due to a bug in the CUPS DDK 1.1.0 ppdmerge program, Japanese
	* PPD files were incorrectly assigned "jp" as the locale name
	* instead of "ja".  Support both the old (incorrect) and new
	* locale names for Japanese...
	*/

	snprintf(lkeyword, sizeof(lkeyword), "jp.%s", keyword);
	attr = ppdFindAttr(ppd, lkeyword, spec);
      }
      else if (!strcmp(ll, "no"))
      {
       /*
	* Norway has two languages, "Bokmal" (the primary one)
	* and "Nynorsk" (new Norwegian); we map "no" to "nb" here as
	* recommended by the locale folks...
	*/

	snprintf(lkeyword, sizeof(lkeyword), "nb.%s", keyword);
	attr = ppdFindAttr(ppd, lkeyword, spec);
      }
    }
  }

#ifdef DEBUG
  if (attr)
    printf("    *%s %s/%s: \"%s\"\n", attr->name, attr->spec, attr->text,
           attr->value ? attr->value : "");
  else
    puts("    NOT FOUND");
#endif /* DEBUG */

  return (attr);
}


/*
 * End of "$Id$".
 */
