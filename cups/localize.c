/*
 * "$Id$"
 *
 *   PPD localization routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
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
 *   ppdLocalize()           - Localize the PPD file to the current locale.
 *   ppdLocalizeIPPReason()  - Get the localized version of a cupsIPPReason
 *                             attribute.
 *   ppdLocalizeMarkerName() - Get the localized version of a marker-names
 *                             attribute value.
 *   _ppdFreeLanguages()     - Free an array of languages from _ppdGetLanguages.
 *   _ppdGetLanguages()      - Get an array of languages from a PPD file.
 *   _ppdHashName()          - Generate a hash value for a device or profile
 *                             name.
 *   _ppdLocalizedAttr()     - Find a localized attribute.
 *   ppd_ll_CC()             - Get the current locale names.
 */

/*
 * Include necessary headers.
 */

#include "globals.h"
#include "ppd-private.h"
#include "debug.h"


/*
 * Local functions...
 */

static void		ppd_ll_CC(char *ll_CC, int ll_CC_size);


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
		ll_CC[6];		/* Language + country locale */


 /*
  * Range check input...
  */

  DEBUG_printf(("ppdLocalize(ppd=%p)\n", ppd));

  if (!ppd)
    return (-1);

 /*
  * Get the default language...
  */

  ppd_ll_CC(ll_CC, sizeof(ll_CC));

 /*
  * Now lookup all of the groups, options, choices, etc.
  */

  for (i = ppd->num_groups, group = ppd->groups; i > 0; i --, group ++)
  {
    if ((locattr = _ppdLocalizedAttr(ppd, "Translation", group->name,
                                     ll_CC)) != NULL)
      strlcpy(group->text, locattr->text, sizeof(group->text));

    for (j = group->num_options, option = group->options; j > 0; j --, option ++)
    {
      if ((locattr = _ppdLocalizedAttr(ppd, "Translation", option->keyword,
                                       ll_CC)) != NULL)
	strlcpy(option->text, locattr->text, sizeof(option->text));

      for (k = option->num_choices, choice = option->choices;
           k > 0;
	   k --, choice ++)
      {
        if (strcmp(choice->choice, "Custom"))
	  locattr = _ppdLocalizedAttr(ppd, option->keyword, choice->choice,
	                              ll_CC);
	else
	{
	  snprintf(ckeyword, sizeof(ckeyword), "Custom%s", option->keyword);

	  locattr = _ppdLocalizedAttr(ppd, ckeyword, "True", ll_CC);
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

      if ((locattr = _ppdLocalizedAttr(ppd, ckeyword, cparam->name,
                                       ll_CC)) != NULL)
        strlcpy(cparam->text, locattr->text, sizeof(cparam->text));
    }
  }

 /*
  * Translate ICC profile names...
  */

  if ((attr = ppdFindAttr(ppd, "APCustomColorMatchingName", NULL)) != NULL)
  {
    if ((locattr = _ppdLocalizedAttr(ppd, "APCustomColorMatchingName",
                                     attr->spec, ll_CC)) != NULL)
      strlcpy(attr->text, locattr->text, sizeof(attr->text));
  }

  for (attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
  {
    cupsArraySave(ppd->sorted_attrs);

    if ((locattr = _ppdLocalizedAttr(ppd, "cupsICCProfile", attr->spec,
                                     ll_CC)) != NULL)
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

    if ((locattr = _ppdLocalizedAttr(ppd, "APPrinterPreset", attr->spec,
                                     ll_CC)) != NULL)
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

  ppd_ll_CC(ll_CC, sizeof(ll_CC));

 /*
  * Find the localized attribute...
  */

  if ((locattr = _ppdLocalizedAttr(ppd, "cupsIPPReason", reason,
                                   ll_CC)) == NULL)
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
 * 'ppdLocalizeMarkerName()' - Get the localized version of a marker-names
 *                             attribute value.
 *
 * This function uses the current locale to find the corresponding name
 * text from the attribute value. If no localized text for the requested
 * name can be found, @code NULL@ is returned.
 *
 * @since CUPS 1.4@
 */

const char *				/* O - Value or @code NULL@ if not found */
ppdLocalizeMarkerName(
    ppd_file_t *ppd,			/* I - PPD file */
    const char *name)			/* I - Marker name to look up */
{
  ppd_attr_t	*locattr;		/* Localized attribute */
  char		ll_CC[6];		/* Language + country locale */


 /*
  * Range check input...
  */

  if (!ppd || !name)
    return (NULL);

 /*
  * Get the default language...
  */

  ppd_ll_CC(ll_CC, sizeof(ll_CC));

 /*
  * Find the localized attribute...
  */

  if ((locattr = _ppdLocalizedAttr(ppd, "cupsMarkerName", name,
                                   ll_CC)) == NULL)
    locattr = ppdFindAttr(ppd, "cupsMarkerName", name);

  return (locattr ? locattr->text : NULL);
}


/*
 * '_ppdFreeLanguages()' - Free an array of languages from _ppdGetLanguages.
 */

void
_ppdFreeLanguages(
    cups_array_t *languages)		/* I - Languages array */
{
  char	*language;			/* Current language */


  for (language = (char *)cupsArrayFirst(languages);
       language;
       language = (char *)cupsArrayNext(languages))
    free(language);

  cupsArrayDelete(languages);
}


/*
 * '_ppdGetLanguages()' - Get an array of languages from a PPD file.
 */

cups_array_t *				/* O - Languages array */
_ppdGetLanguages(ppd_file_t *ppd)	/* I - PPD file */
{
  cups_array_t	*languages;		/* Languages array */
  ppd_attr_t	*attr;			/* cupsLanguages attribute */
  char		*value,			/* Copy of attribute value */
		*start,			/* Start of current language */
		*ptr;			/* Pointer into languages */


 /*
  * See if we have a cupsLanguages attribute...
  */

  if ((attr = ppdFindAttr(ppd, "cupsLanguages", NULL)) == NULL || !attr->value)
    return (NULL);

 /*
  * Yes, load the list...
  */

  if ((languages = cupsArrayNew((cups_array_func_t)strcmp, NULL)) == NULL)
    return (NULL);

  if ((value = strdup(attr->value)) == NULL)
  {
    cupsArrayDelete(languages);
    return (NULL);
  }

  for (ptr = value; *ptr;)
  {
   /*
    * Skip leading whitespace...
    */

    while (isspace(*ptr & 255))
      ptr ++;

    if (!*ptr)
      break;

   /*
    * Find the end of this language name...
    */

    for (start = ptr; *ptr && !isspace(*ptr & 255); ptr ++);

    if (*ptr)
      *ptr++ = '\0';

    if (!strcmp(start, "en"))
      continue;

    cupsArrayAdd(languages, strdup(start));
  }

 /*
  * Free the temporary string and return either an array with one or more
  * values or a NULL pointer...
  */

  free(value);

  if (cupsArrayCount(languages) == 0)
  {
    cupsArrayDelete(languages);
    return (NULL);
  }
  else
    return (languages);
}


/*
 * '_ppdHashName()' - Generate a hash value for a device or profile name.
 *
 * This function is primarily used on Mac OS X, but is generally accessible
 * since cupstestppd needs to check for profile name collisions in PPD files...
 */

unsigned				/* O - Hash value */
_ppdHashName(const char *name)		/* I - Name to hash */
{
  int		mult;			/* Multiplier */
  unsigned	hash = 0;		/* Hash value */


  for (mult = 1; *name && mult <= 128; mult ++, name ++)
    hash += (*name & 255) * mult;

  return (hash);
}


/*
 * '_ppdLocalizedAttr()' - Find a localized attribute.
 */

ppd_attr_t *				/* O - Localized attribute or NULL */
_ppdLocalizedAttr(ppd_file_t *ppd,	/* I - PPD file */
		  const char *keyword,	/* I - Main keyword */
		  const char *spec,	/* I - Option keyword */
		  const char *ll_CC)	/* I - Language + country locale */
{
  char		lkeyword[PPD_MAX_NAME];	/* Localization keyword */
  ppd_attr_t	*attr;			/* Current attribute */


  DEBUG_printf(("_ppdLocalizedAttr(ppd=%p, keyword=\"%s\", spec=\"%s\", "
                "ll_CC=\"%s\")\n", ppd, keyword, spec, ll_CC));

 /*
  * Look for Keyword.ll_CC, then Keyword.ll...
  */

  snprintf(lkeyword, sizeof(lkeyword), "%s.%s", ll_CC, keyword);
  if ((attr = ppdFindAttr(ppd, lkeyword, spec)) == NULL)
  {
    snprintf(lkeyword, sizeof(lkeyword), "%2s.%s", ll_CC, keyword);
    attr = ppdFindAttr(ppd, lkeyword, spec);

    if (!attr)
    {
      if (!strncmp(ll_CC, "ja", 2))
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
      else if (!strncmp(ll_CC, "no", 2))
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
    DEBUG_printf(("_ppdLocalizedAttr: *%s %s/%s: \"%s\"\n", attr->name,
                  attr->spec, attr->text, attr->value ? attr->value : ""));
  else
    DEBUG_puts("_ppdLocalizedAttr: NOT FOUND");
#endif /* DEBUG */

  return (attr);
}


/*
 * 'ppd_ll_CC()' - Get the current locale names.
 */

static void
ppd_ll_CC(char *ll_CC,			/* O - Country-specific locale name */
          int  ll_CC_size)		/* I - Size of country-specific name */
{
  cups_lang_t	*lang;			/* Current language */


 /*
  * Get the current locale...
  */

  if ((lang = cupsLangDefault()) == NULL)
  {
    strlcpy(ll_CC, "en_US", ll_CC_size);
    return;
  }

 /*
  * Copy the locale name...
  */

  strlcpy(ll_CC, lang->language, ll_CC_size);

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

  DEBUG_printf(("ppd_ll_CC: lang->language=\"%s\", ll_CC=\"%s\"...\n",
                lang->language, ll_CC));
}


/*
 * End of "$Id$".
 */
