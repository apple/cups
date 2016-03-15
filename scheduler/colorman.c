/*
 * "$Id: colorman.c 12369 2014-12-15 14:51:28Z msweet $"
 *
 * Color management routines for the CUPS scheduler.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Original DBUS/colord code is Copyright 2011 Red Hat, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"
#include <cups/ppd-private.h>

#ifdef __APPLE__
#  include <ApplicationServices/ApplicationServices.h>
extern CFUUIDRef ColorSyncCreateUUIDFromUInt32(unsigned id);
#  include <CoreFoundation/CoreFoundation.h>
#elif defined(HAVE_DBUS)
#  include <dbus/dbus.h>

/*
 * Defines used by colord. See the reference docs for further details:
 *
 *   http://colord.hughsie.com/api/ref-dbus.html
 */

#  define COLORD_SCOPE_NORMAL	"normal"
					/* System scope */
#  define COLORD_SCOPE_TEMP	"temp"	/* Process scope */
#  define COLORD_SCOPE_DISK	"disk"	/* Lives forever, as stored in DB */

#  define COLORD_RELATION_SOFT	"soft"	/* Mapping is not default */
#  define COLORD_RELATION_HARD	"hard"	/* Explicitly mapped profile */

#  define COLORD_SPACE_RGB	"rgb"	/* RGB colorspace */
#  define COLORD_SPACE_CMYK	"cmyk"	/* CMYK colorspace */
#  define COLORD_SPACE_GRAY	"gray"	/* Gray colorspace */
#  define COLORD_SPACE_UNKNOWN	"unknown"
					/* Unknown colorspace */

#  define COLORD_MODE_PHYSICAL	"physical"
					/* Actual device */
#  define COLORD_MODE_VIRTUAL	"virtual"
					/* Virtual device with no hardware */

#  define COLORD_KIND_PRINTER	"printer"
					/* printing output device */

#  define COLORD_DBUS_SERVICE		"org.freedesktop.ColorManager"
#  define COLORD_DBUS_INTERFACE 	"org.freedesktop.ColorManager"
#  define COLORD_DBUS_INTERFACE_DEVICE	"org.freedesktop.ColorManager.Device"
#  define COLORD_DBUS_PATH		"/org/freedesktop/ColorManager"
					/* Path for color management system */
#  define COLORD_DBUS_TIMEOUT	5000	/* Timeout for connecting to colord in ms */
#endif /* __APPLE__ */


/*
 * Local globals...
 */

#if !defined(__APPLE__) && defined(HAVE_DBUS)
static DBusConnection *colord_con = NULL;
					/* DBUS connection for colord */
#endif /* !__APPLE__ && HAVE_DBUS */


/*
 * Local functions...
 */

#ifdef __APPLE__
static void	apple_init_profile(ppd_file_t *ppd, cups_array_t *languages,
                                   CFMutableDictionaryRef profile,
				   unsigned id, const char *name,
				   const char *text, const char *iccfile);
static void	apple_register_profiles(cupsd_printer_t *p);
static void	apple_unregister_profiles(cupsd_printer_t *p);

#elif defined(HAVE_DBUS)
static void	colord_create_device(cupsd_printer_t *p, ppd_file_t *ppd,
				     cups_array_t *profiles,
				     const char *colorspace, char **format,
				     const char *relation, const char *scope);
static void	colord_create_profile(cups_array_t *profiles,
				      const char *printer_name,
				      const char *qualifier,
				      const char *colorspace,
				      char **format, const char *iccfile,
				      const char *scope);
static void	colord_delete_device(const char *device_id);
static void	colord_device_add_profile(const char *device_path,
					  const char *profile_path,
					  const char *relation);
static void	colord_dict_add_strings(DBusMessageIter *dict,
					const char *key, const char *value);
static char	*colord_find_device(const char *device_id);
static void	colord_get_qualifier_format(ppd_file_t *ppd, char *format[3]);
static void	colord_register_printer(cupsd_printer_t *p);
static void	colord_unregister_printer(cupsd_printer_t *p);
#endif /* __APPLE__ */


/*
 * 'cupsdRegisterColor()' - Register vendor color profiles in a PPD file.
 */

void
cupsdRegisterColor(cupsd_printer_t *p)	/* I - Printer */
{
#ifdef __APPLE__
  if (!RunUser)
  {
    apple_unregister_profiles(p);
    apple_register_profiles(p);
  }

#elif defined(HAVE_DBUS)
  if (!RunUser)
  {
    colord_unregister_printer(p);
    colord_register_printer(p);
  }
#endif /* __APPLE__ */
}


/*
 * 'cupsdStartColor()' - Initialize color management.
 */

void
cupsdStartColor(void)
{
#if !defined(__APPLE__) && defined(HAVE_DBUS)
  cupsd_printer_t	*p;		/* Current printer */


  colord_con = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

  for (p = (cupsd_printer_t *)cupsArrayFirst(Printers);
       p;
       p = (cupsd_printer_t *)cupsArrayNext(Printers))
    cupsdRegisterColor(p);
#endif /* !__APPLE__ && HAVE_DBUS */
}


/*
 * 'cupsdStopColor()' - Shutdown color management.
 */

void
cupsdStopColor(void)
{
#if !defined(__APPLE__) && defined(HAVE_DBUS)
  if (colord_con)
    dbus_connection_unref(colord_con);
  colord_con = NULL;
#endif /* !__APPLE__ && HAVE_DBUS */
}


/*
 * 'cupsdUnregisterColor()' - Unregister vendor color profiles in a PPD file.
 */

void
cupsdUnregisterColor(cupsd_printer_t *p)/* I - Printer */
{
#ifdef __APPLE__
  if (!RunUser)
    apple_unregister_profiles(p);

#elif defined(HAVE_DBUS)
  if (!RunUser)
    colord_unregister_printer(p);
#endif /* __APPLE__ */
}


#ifdef __APPLE__
/*
 * 'apple_init_profile()' - Initialize a color profile.
 */

static void
apple_init_profile(
    ppd_file_t             *ppd,	/* I - PPD file */
    cups_array_t	   *languages,	/* I - Languages in the PPD file */
    CFMutableDictionaryRef profile,	/* I - Profile dictionary */
    unsigned               id,		/* I - Profile ID */
    const char             *name,	/* I - Profile name */
    const char             *text,	/* I - Profile UI text */
    const char             *iccfile)	/* I - ICC filename */
{
  CFURLRef		url;		/* URL for profile filename */
  CFMutableDictionaryRef dict;		/* Dictionary for name */
  char			*language;	/* Current language */
  ppd_attr_t		*attr;		/* Profile attribute */
  CFStringRef		cflang,		/* Language string */
			cftext;		/* Localized text */


  (void)id;

 /*
  * Build the profile name dictionary...
  */

  dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);
  if (!dict)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR, "Unable to initialize profile \"%s\".",
                    iccfile);
    return;
  }

  cftext = CFStringCreateWithCString(kCFAllocatorDefault, text,
				     kCFStringEncodingUTF8);

  if (cftext)
  {
    CFDictionarySetValue(dict, CFSTR("en_US"), cftext);
    CFRelease(cftext);
  }

  if (languages)
  {
   /*
    * Find localized names for the color profiles...
    */

    cupsArraySave(ppd->sorted_attrs);

    for (language = (char *)cupsArrayFirst(languages);
	 language;
	 language = (char *)cupsArrayNext(languages))
    {
      if (iccfile)
      {
        if ((attr = _ppdLocalizedAttr(ppd, "cupsICCProfile", name,
	                              language)) == NULL)
	  attr = _ppdLocalizedAttr(ppd, "APTiogaProfile", name, language);
      }
      else
        attr = _ppdLocalizedAttr(ppd, "ColorModel", name, language);

      if (attr && attr->text[0])
      {
	cflang = CFStringCreateWithCString(kCFAllocatorDefault, language,
					   kCFStringEncodingUTF8);
	cftext = CFStringCreateWithCString(kCFAllocatorDefault, attr->text,
					   kCFStringEncodingUTF8);

        if (cflang && cftext)
	  CFDictionarySetValue(dict, cflang, cftext);

        if (cflang)
	  CFRelease(cflang);

        if (cftext)
	  CFRelease(cftext);
      }
    }

    cupsArrayRestore(ppd->sorted_attrs);
  }

 /*
  * Fill in the profile data...
  */

 if (iccfile && *iccfile)
 {
    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)iccfile, (CFIndex)strlen(iccfile), false);

    if (url)
    {
      CFDictionarySetValue(profile, kColorSyncDeviceProfileURL, url);
      CFRelease(url);
    }
  }

  CFDictionarySetValue(profile, kColorSyncDeviceModeDescriptions, dict);
  CFRelease(dict);
}


/*
 * 'apple_register_profiles()' - Register color profiles for a printer.
 */

static void
apple_register_profiles(
    cupsd_printer_t *p)			/* I - Printer */
{
  int			i;		/* Looping var */
  char			ppdfile[1024],	/* PPD filename */
			iccfile[1024],	/* ICC filename */
			selector[PPD_MAX_NAME];
					/* Profile selection string */
  ppd_file_t		*ppd;		/* PPD file */
  ppd_attr_t		*attr,		/* Profile attributes */
			*profileid_attr,/* cupsProfileID attribute */
			*q1_attr,	/* ColorModel (or other) qualifier */
			*q2_attr,	/* MediaType (or other) qualifier */
			*q3_attr;	/* Resolution (or other) qualifier */
  char			q_keyword[PPD_MAX_NAME];
					/* Qualifier keyword */
  const char		*q1_choice,	/* ColorModel (or other) choice */
			*q2_choice,	/* MediaType (or other) choice */
			*q3_choice;	/* Resolution (or other) choice */
  ppd_option_t		*cm_option;	/* Color model option */
  ppd_choice_t		*cm_choice;	/* Color model choice */
  int			num_profiles;	/* Number of profiles */
  OSStatus		error = 0;	/* Last error */
  unsigned		device_id,	/* Printer device ID */
			profile_id = 0,	/* Profile ID */
			default_profile_id = 0;
					/* Default profile ID */
  CFMutableDictionaryRef device_name;	/* Printer device name dictionary */
  CFStringRef		printer_name;	/* Printer name string */
  cups_array_t		*languages;	/* Languages array */
  CFMutableDictionaryRef profiles,	/* Dictionary of profiles */
			profile;	/* Current profile info dictionary */
  CFStringRef		dict_key;	/* Key in factory profile dictionary */


 /*
  * Make sure ColorSync is available...
  */

  if (ColorSyncRegisterDevice == NULL)
    return;

 /*
  * Try opening the PPD file for this printer...
  */

  snprintf(ppdfile, sizeof(ppdfile), "%s/ppd/%s.ppd", ServerRoot, p->name);
  if ((ppd = _ppdOpenFile(ppdfile, _PPD_LOCALIZATION_ICC_PROFILES)) == NULL)
    return;

 /*
  * See if we have any profiles...
  */

  for (num_profiles = 0, attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
    if (attr->spec[0] && attr->value && attr->value[0])
    {
      if (attr->value[0] != '/')
	snprintf(iccfile, sizeof(iccfile), "%s/profiles/%s", DataDir,
		 attr->value);
      else
	strlcpy(iccfile, attr->value, sizeof(iccfile));

      if (access(iccfile, 0))
      {
        cupsdLogMessage(CUPSD_LOG_ERROR,
                        "%s: ICC Profile \"%s\" does not exist.", p->name,
                        iccfile);
        cupsdSetPrinterReasons(p, "+cups-missing-filter-warning");
	continue;
      }

      num_profiles ++;
    }

 /*
  * Create a dictionary for the factory profiles...
  */

  profiles = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				       &kCFTypeDictionaryKeyCallBacks,
				       &kCFTypeDictionaryValueCallBacks);
  if (!profiles)
  {
    cupsdLogMessage(CUPSD_LOG_ERROR,
		    "Unable to allocate memory for factory profiles.");
    ppdClose(ppd);
    return;
  }

 /*
  * If we have profiles, add them...
  */

  if (num_profiles > 0)
  {
   /*
    * For CUPS PPDs, figure out the default profile selector values...
    */

    if ((attr = ppdFindAttr(ppd, "cupsICCQualifier1", NULL)) != NULL &&
	attr->value && attr->value[0])
    {
      snprintf(q_keyword, sizeof(q_keyword), "Default%s", attr->value);
      q1_attr = ppdFindAttr(ppd, q_keyword, NULL);
    }
    else if ((q1_attr = ppdFindAttr(ppd, "DefaultColorModel", NULL)) == NULL)
      q1_attr = ppdFindAttr(ppd, "DefaultColorSpace", NULL);

    if (q1_attr && q1_attr->value && q1_attr->value[0])
      q1_choice = q1_attr->value;
    else
      q1_choice = "";

    if ((attr = ppdFindAttr(ppd, "cupsICCQualifier2", NULL)) != NULL &&
	attr->value && attr->value[0])
    {
      snprintf(q_keyword, sizeof(q_keyword), "Default%s", attr->value);
      q2_attr = ppdFindAttr(ppd, q_keyword, NULL);
    }
    else
      q2_attr = ppdFindAttr(ppd, "DefaultMediaType", NULL);

    if (q2_attr && q2_attr->value && q2_attr->value[0])
      q2_choice = q2_attr->value;
    else
      q2_choice = NULL;

    if ((attr = ppdFindAttr(ppd, "cupsICCQualifier3", NULL)) != NULL &&
	attr->value && attr->value[0])
    {
      snprintf(q_keyword, sizeof(q_keyword), "Default%s", attr->value);
      q3_attr = ppdFindAttr(ppd, q_keyword, NULL);
    }
    else
      q3_attr = ppdFindAttr(ppd, "DefaultResolution", NULL);

    if (q3_attr && q3_attr->value && q3_attr->value[0])
      q3_choice = q3_attr->value;
    else
      q3_choice = NULL;

   /*
    * Loop through the profiles listed in the PPD...
    */

    languages = _ppdGetLanguages(ppd);

    for (attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
	 attr;
	 attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
      if (attr->spec[0] && attr->value && attr->value[0])
      {
       /*
        * Add this profile...
	*/

        if (attr->value[0] != '/')
	  snprintf(iccfile, sizeof(iccfile), "%s/profiles/%s", DataDir,
	           attr->value);
        else
	  strlcpy(iccfile, attr->value, sizeof(iccfile));

        if (_cupsFileCheck(iccfile, _CUPS_FILE_CHECK_FILE, !RunUser,
	                   cupsdLogFCMessage, p))
	  iccfile[0] = '\0';

	cupsArraySave(ppd->sorted_attrs);

	if ((profileid_attr = ppdFindAttr(ppd, "cupsProfileID",
					  attr->spec)) != NULL &&
	    profileid_attr->value && isdigit(profileid_attr->value[0] & 255))
	  profile_id = (unsigned)strtoul(profileid_attr->value, NULL, 10);
	else
	  profile_id = _ppdHashName(attr->spec);

	cupsArrayRestore(ppd->sorted_attrs);

	profile = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	if (!profile)
	{
	  cupsdLogMessage(CUPSD_LOG_ERROR,
	                  "Unable to allocate memory for color profile.");
	  CFRelease(profiles);
	  ppdClose(ppd);
	  return;
	}

	apple_init_profile(ppd, languages, profile, profile_id, attr->spec,
	                   attr->text[0] ? attr->text : attr->spec, iccfile);

	dict_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
	                                    CFSTR("%u"), profile_id);
	if (dict_key)
	{
	  CFDictionarySetValue(profiles, dict_key, profile);
	  CFRelease(dict_key);
	}

	CFRelease(profile);

       /*
        * See if this is the default profile...
	*/

        if (!default_profile_id && q1_choice && q2_choice && q3_choice)
	{
	  snprintf(selector, sizeof(selector), "%s.%s.%s", q1_choice, q2_choice,
	           q3_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q1_choice && q2_choice)
	{
	  snprintf(selector, sizeof(selector), "%s.%s.", q1_choice, q2_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q1_choice && q3_choice)
	{
	  snprintf(selector, sizeof(selector), "%s..%s", q1_choice, q3_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q1_choice)
	{
	  snprintf(selector, sizeof(selector), "%s..", q1_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q2_choice && q3_choice)
	{
	  snprintf(selector, sizeof(selector), ".%s.%s", q2_choice, q3_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q2_choice)
	{
	  snprintf(selector, sizeof(selector), ".%s.", q2_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}

        if (!default_profile_id && q3_choice)
	{
	  snprintf(selector, sizeof(selector), "..%s", q3_choice);
	  if (!strcmp(selector, attr->spec))
	    default_profile_id = profile_id;
	}
      }

    _ppdFreeLanguages(languages);
  }
  else if ((cm_option = ppdFindOption(ppd, "ColorModel")) != NULL)
  {
   /*
    * Extract profiles from ColorModel option...
    */

    const char *profile_name;		/* Name of generic profile */


    num_profiles = cm_option->num_choices;

    for (i = cm_option->num_choices, cm_choice = cm_option->choices;
         i > 0;
	 i --, cm_choice ++)
    {
      if (!strcmp(cm_choice->choice, "Gray") ||
          !strcmp(cm_choice->choice, "Black"))
        profile_name = "Gray";
      else if (!strcmp(cm_choice->choice, "RGB") ||
               !strcmp(cm_choice->choice, "CMY"))
        profile_name = "RGB";
      else if (!strcmp(cm_choice->choice, "CMYK") ||
               !strcmp(cm_choice->choice, "KCMY"))
        profile_name = "CMYK";
      else
        profile_name = "DeviceN";

      snprintf(selector, sizeof(selector), "%s..", profile_name);
      profile_id = _ppdHashName(selector);

      profile = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
      if (!profile)
      {
	cupsdLogMessage(CUPSD_LOG_ERROR,
			"Unable to allocate memory for color profile.");
	CFRelease(profiles);
	ppdClose(ppd);
	return;
      }

      apple_init_profile(ppd, NULL, profile, profile_id, cm_choice->choice,
                         cm_choice->text, NULL);

      dict_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                          CFSTR("%u"), profile_id);
      if (dict_key)
      {
	CFDictionarySetValue(profiles, dict_key, profile);
	CFRelease(dict_key);
      }

      CFRelease(profile);

      if (cm_choice->marked)
        default_profile_id = profile_id;
    }
  }
  else
  {
   /*
    * Use the default colorspace...
    */

    attr = ppdFindAttr(ppd, "DefaultColorSpace", NULL);

    num_profiles = (attr && ppd->colorspace == PPD_CS_GRAY) ? 1 : 2;

   /*
    * Add the grayscale profile first.  We always have a grayscale profile.
    */

    profile = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				        &kCFTypeDictionaryKeyCallBacks,
				        &kCFTypeDictionaryValueCallBacks);

    if (!profile)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for color profile.");
      CFRelease(profiles);
      ppdClose(ppd);
      return;
    }

    profile_id = _ppdHashName("Gray..");
    apple_init_profile(ppd, NULL, profile, profile_id, "Gray", "Gray", NULL);

    dict_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%u"),
                                        profile_id);
    if (dict_key)
    {
      CFDictionarySetValue(profiles, dict_key, profile);
      CFRelease(dict_key);
    }

    CFRelease(profile);

   /*
    * Then add the RGB/CMYK/DeviceN color profile...
    */

    profile = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				        &kCFTypeDictionaryKeyCallBacks,
				        &kCFTypeDictionaryValueCallBacks);

    if (!profile)
    {
      cupsdLogMessage(CUPSD_LOG_ERROR,
                      "Unable to allocate memory for color profile.");
      CFRelease(profiles);
      ppdClose(ppd);
      return;
    }

    switch (ppd->colorspace)
    {
      default :
      case PPD_CS_RGB :
      case PPD_CS_CMY :
          profile_id = _ppdHashName("RGB..");
          apple_init_profile(ppd, NULL, profile, profile_id, "RGB", "RGB",
	                     NULL);
          break;

      case PPD_CS_RGBK :
      case PPD_CS_CMYK :
          profile_id = _ppdHashName("CMYK..");
          apple_init_profile(ppd, NULL, profile, profile_id, "CMYK", "CMYK",
	                     NULL);
          break;

      case PPD_CS_GRAY :
          if (attr)
            break;

      case PPD_CS_N :
          profile_id = _ppdHashName("DeviceN..");
          apple_init_profile(ppd, NULL, profile, profile_id, "DeviceN",
	                     "DeviceN", NULL);
          break;
    }

    if (CFDictionaryGetCount(profile) > 0)
    {
      dict_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                          CFSTR("%u"), profile_id);
      if (dict_key)
      {
        CFDictionarySetValue(profiles, dict_key, profile);
        CFRelease(dict_key);
      }
    }

    CFRelease(profile);
  }

  if (num_profiles > 0)
  {
   /*
    * Make sure we have a default profile ID...
    */

    if (!default_profile_id)
      default_profile_id = profile_id;	/* Last profile */

    dict_key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%u"),
                                        default_profile_id);
    if (dict_key)
    {
      CFDictionarySetValue(profiles, kColorSyncDeviceDefaultProfileID,
                           dict_key);
      CFRelease(dict_key);
    }

   /*
    * Get the device ID hash and pathelogical name dictionary.
    */

    cupsdLogMessage(CUPSD_LOG_INFO, "Registering ICC color profiles for \"%s\"",
		    p->name);

    device_id    = _ppdHashName(p->name);
    device_name  = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
    printer_name = CFStringCreateWithCString(kCFAllocatorDefault,
                                             p->name, kCFStringEncodingUTF8);

    if (device_name && printer_name)
    {
     /*
      * Register the device with ColorSync...
      */

      CFTypeRef		deviceDictKeys[] =
      {					/* Device keys */
        kColorSyncDeviceDescriptions,
	kColorSyncFactoryProfiles,
	kColorSyncDeviceUserScope,
	kColorSyncDeviceHostScope
      };
      CFTypeRef 	deviceDictVals[] =
      {					/* Device values */
        device_name,
	profiles,
	kCFPreferencesAnyUser,
	kCFPreferencesCurrentHost
      };
      CFDictionaryRef	deviceDict;	/* Device dictionary */
      CFUUIDRef		deviceUUID;	/* Device UUID */

      CFDictionarySetValue(device_name, CFSTR("en_US"), printer_name);

      deviceDict = CFDictionaryCreate(kCFAllocatorDefault,
				      (const void **)deviceDictKeys,
				      (const void **)deviceDictVals,
				      sizeof(deviceDictKeys) /
				          sizeof(deviceDictKeys[0]),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
      deviceUUID = ColorSyncCreateUUIDFromUInt32(device_id);

      if (!deviceDict || !deviceUUID ||
	  !ColorSyncRegisterDevice(kColorSyncPrinterDeviceClass, deviceUUID,
				   deviceDict))
	error = 1001;

      if (deviceUUID)
        CFRelease(deviceUUID);

      if (deviceDict)
        CFRelease(deviceDict);
    }
    else
      error = 1000;

   /*
    * Clean up...
    */

    if (error != noErr)
      cupsdLogMessage(CUPSD_LOG_ERROR,
		      "Unable to register ICC color profiles for \"%s\": %d",
		      p->name, (int)error);

    if (printer_name)
      CFRelease(printer_name);

    if (device_name)
      CFRelease(device_name);
  }

 /*
  * Free any memory we used...
  */

  CFRelease(profiles);

  ppdClose(ppd);
}


/*
 * 'apple_unregister_profiles()' - Remove color profiles for the specified
 *                                 printer.
 */

static void
apple_unregister_profiles(
    cupsd_printer_t *p)			/* I - Printer */
{
 /*
  * Make sure ColorSync is available...
  */

  if (ColorSyncUnregisterDevice != NULL)
  {
    CFUUIDRef deviceUUID;		/* Device UUID */

    deviceUUID = ColorSyncCreateUUIDFromUInt32(_ppdHashName(p->name));
    if (deviceUUID)
    {
      ColorSyncUnregisterDevice(kColorSyncPrinterDeviceClass, deviceUUID);
      CFRelease(deviceUUID);
    }
  }
}


#elif defined(HAVE_DBUS)
/*
 * 'colord_create_device()' - Create a device and register profiles.
 */

static void
colord_create_device(
    cupsd_printer_t *p,			/* I - Printer */
    ppd_file_t      *ppd,		/* I - PPD file */
    cups_array_t    *profiles,		/* I - Profiles array */
    const char      *colorspace,	/* I - Device colorspace, e.g. 'rgb' */
    char            **format,		/* I - Device qualifier format */
    const char      *relation,		/* I - Profile relation, either 'soft'
					       or 'hard' */
    const char      *scope)		/* I - The scope of the device, e.g.
					       'normal', 'temp' or 'disk' */
{
  DBusMessage	*message = NULL;	/* D-Bus request */
  DBusMessage	*reply = NULL;		/* D-Bus reply */
  DBusMessageIter args;			/* D-Bus method arguments */
  DBusMessageIter dict;			/* D-Bus method arguments */
  DBusError	error;			/* D-Bus error */
  const char	*device_path;		/* Device object path */
  const char	*profile_path;		/* Profile path */
  char		*default_profile_path = NULL;
					/* Default profile path */
  char		device_id[1024];	/* Device ID as understood by colord */
  char		format_str[1024];	/* Qualifier format as a string */


 /*
  * Create the device...
  */

  snprintf(device_id, sizeof(device_id), "cups-%s", p->name);
  device_path = device_id;

  message = dbus_message_new_method_call(COLORD_DBUS_SERVICE,
                                         COLORD_DBUS_PATH,
                                         COLORD_DBUS_INTERFACE,
                                         "CreateDevice");

  dbus_message_iter_init_append(message, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &device_path);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &scope);

  snprintf(format_str, sizeof(format_str), "%s.%s.%s", format[0], format[1],
           format[2]);

  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{ss}", &dict);
  colord_dict_add_strings(&dict, "Colorspace", colorspace);
  colord_dict_add_strings(&dict, "Mode", COLORD_MODE_PHYSICAL);
  if (ppd->manufacturer)
    colord_dict_add_strings(&dict, "Vendor", ppd->manufacturer);
  if (ppd->modelname)
    colord_dict_add_strings(&dict, "Model", ppd->modelname);
  if (p->sanitized_device_uri)
    colord_dict_add_strings(&dict, "Serial", p->sanitized_device_uri);
  colord_dict_add_strings(&dict, "Format", format_str);
  colord_dict_add_strings(&dict, "Kind", COLORD_KIND_PRINTER);
  dbus_message_iter_close_container(&args, &dict);

 /*
  * Send the CreateDevice request synchronously...
  */

  dbus_error_init(&error);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Calling CreateDevice(%s,%s)", device_id,
                  scope);
  reply = dbus_connection_send_with_reply_and_block(colord_con, message,
                                                    COLORD_DBUS_TIMEOUT,
                                                    &error);
  if (!reply)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "CreateDevice failed: %s:%s", error.name,
                    error.message);
    dbus_error_free(&error);
    goto out;
  }

 /*
  * Get reply data...
  */

  dbus_message_iter_init(reply, &args);
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "CreateDevice failed: Incorrect reply type.");
    goto out;
  }

  dbus_message_iter_get_basic(&args, &device_path);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Created device \"%s\".", device_path);

 /*
  * Add profiles...
  */

  for (profile_path = cupsArrayFirst(profiles);
       profile_path;
       profile_path = cupsArrayNext(profiles))
  {
    colord_device_add_profile(device_path, profile_path, relation);
  }

out:

  if (default_profile_path)
    free(default_profile_path);

  if (message)
    dbus_message_unref(message);

  if (reply)
    dbus_message_unref(reply);
}


/*
 * 'colord_create_profile()' - Create a color profile for a printer.
 */

static void
colord_create_profile(
    cups_array_t *profiles,		/* I - Profiles array */
    const char   *printer_name,		/* I - Printer name */
    const char   *qualifier,		/* I - Profile qualifier */
    const char   *colorspace,		/* I - Profile colorspace */
    char         **format,		/* I - Profile qualifier format */
    const char   *iccfile,		/* I - ICC filename */
    const char   *scope)		/* I - The scope of the profile, e.g.
				               'normal', 'temp' or 'disk' */
{
  DBusMessage	*message = NULL;        /* D-Bus request */
  DBusMessage	*reply = NULL;          /* D-Bus reply */
  DBusMessageIter args;			/* D-Bus method arguments */
  DBusMessageIter dict;			/* D-Bus method arguments */
  DBusError	error;			/* D-Bus error */
  char		*idstr;			/* Profile ID string */
  size_t	idstrlen;		/* Profile ID allocated length */
  const char	*profile_path;		/* Device object path */
  char		format_str[1024];	/* Qualifier format as a string */


 /*
  * Create the profile...
  */

  message = dbus_message_new_method_call(COLORD_DBUS_SERVICE,
                                         COLORD_DBUS_PATH,
                                         COLORD_DBUS_INTERFACE,
                                         "CreateProfile");

  idstrlen = strlen(printer_name) + 1 + strlen(qualifier) + 1;
  if ((idstr = malloc(idstrlen)) == NULL)
    goto out;
  snprintf(idstr, idstrlen, "%s-%s", printer_name, qualifier);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Using profile ID \"%s\".", idstr);

  dbus_message_iter_init_append(message, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &idstr);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &scope);

  snprintf(format_str, sizeof(format_str), "%s.%s.%s", format[0], format[1],
           format[2]);

  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{ss}", &dict);
  colord_dict_add_strings(&dict, "Qualifier", qualifier);
  colord_dict_add_strings(&dict, "Format", format_str);
  colord_dict_add_strings(&dict, "Colorspace", colorspace);
  if (iccfile)
    colord_dict_add_strings(&dict, "Filename", iccfile);
  dbus_message_iter_close_container(&args, &dict);

 /*
  * Send the CreateProfile request synchronously...
  */

  dbus_error_init(&error);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Calling CreateProfile(%s,%s)", idstr,
                  scope);
  reply = dbus_connection_send_with_reply_and_block(colord_con, message,
                                                    COLORD_DBUS_TIMEOUT,
                                                    &error);
  if (!reply)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "CreateProfile failed: %s:%s", error.name,
                    error.message);
    dbus_error_free(&error);
    goto out;
  }

 /*
  * Get reply data...
  */

  dbus_message_iter_init(reply, &args);
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "CreateProfile failed: Incorrect reply type.");
    goto out;
  }

  dbus_message_iter_get_basic(&args, &profile_path);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Created profile \"%s\".", profile_path);
  cupsArrayAdd(profiles, strdup(profile_path));

out:

  if (message)
    dbus_message_unref(message);

  if (reply)
    dbus_message_unref(reply);

  if (idstr)
    free(idstr);
}


/*
 * 'colord_delete_device()' - Delete a device
 */

static void
colord_delete_device(
    const char *device_id)		/* I - Device ID string */
{
  DBusMessage	*message = NULL;	/* D-Bus request */
  DBusMessage	*reply = NULL;		/* D-Bus reply */
  DBusMessageIter args;			/* D-Bus method arguments */
  DBusError	error;			/* D-Bus error */
  char		*device_path;		/* Device object path */


 /*
  * Find the device...
  */

  if ((device_path = colord_find_device(device_id)) == NULL)
    goto out;

 /*
  * Delete the device...
  */

  message = dbus_message_new_method_call(COLORD_DBUS_SERVICE,
                                         COLORD_DBUS_PATH,
                                         COLORD_DBUS_INTERFACE,
                                         "DeleteDevice");

  dbus_message_iter_init_append(message, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &device_path);

 /*
  * Send the DeleteDevice request synchronously...
  */

  dbus_error_init(&error);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Calling DeleteDevice(%s)", device_path);
  reply = dbus_connection_send_with_reply_and_block(colord_con, message,
                                                    COLORD_DBUS_TIMEOUT,
                                                    &error);
  if (!reply)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "DeleteDevice failed: %s:%s", error.name,
                    error.message);
    dbus_error_free(&error);
    goto out;
  }

out:

  if (device_path)
    free(device_path);

  if (message)
    dbus_message_unref(message);

  if (reply)
    dbus_message_unref(reply);
}


/*
 * 'colord_device_add_profile()' - Assign a profile to a device.
 */

static void
colord_device_add_profile(
    const char *device_path,		/* I - Device object path */
    const char *profile_path,		/* I - Profile object path */
    const char *relation)		/* I - Device relation, either
					       'soft' or 'hard' */
{
  DBusMessage	*message = NULL;	/* D-Bus request */
  DBusMessage	*reply = NULL;		/* D-Bus reply */
  DBusMessageIter args;			/* D-Bus method arguments */
  DBusError	error;			/* D-Bus error */


  message = dbus_message_new_method_call(COLORD_DBUS_SERVICE,
                                         device_path,
                                         COLORD_DBUS_INTERFACE_DEVICE,
                                         "AddProfile");

  dbus_message_iter_init_append(message, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &relation);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &profile_path);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Calling %s:AddProfile(%s) [%s]",
                  device_path, profile_path, relation);

 /*
  * Send the AddProfile request synchronously...
  */

  dbus_error_init(&error);
  reply = dbus_connection_send_with_reply_and_block(colord_con, message,
                                                    COLORD_DBUS_TIMEOUT,
                                                    &error);
  if (!reply)
  {
    cupsdLogMessage(CUPSD_LOG_WARN, "AddProfile failed: %s:%s", error.name,
                    error.message);
    dbus_error_free(&error);
    goto out;
  }

out:

  if (message)
    dbus_message_unref(message);

  if (reply)
    dbus_message_unref(reply);
}


/*
 * 'colord_dict_add_strings()' - Add two strings to a dictionary.
 */

static void
colord_dict_add_strings(
    DBusMessageIter *dict,		/* I - Dictionary */
    const char      *key,		/* I - Key string */
    const char      *value)		/* I - Value string */
{
  DBusMessageIter	entry;		/* Entry to add */


  dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &value);
  dbus_message_iter_close_container(dict, &entry);
}


/*
 * 'colord_find_device()' - Finds a device
 */

static char *				/* O - Device path or NULL */
colord_find_device(
    const char *device_id)		/* I - Device ID string */
{
  DBusMessage	*message = NULL;	/* D-Bus request */
  DBusMessage	*reply = NULL;		/* D-Bus reply */
  DBusMessageIter args;			/* D-Bus method arguments */
  DBusError	error;			/* D-Bus error */
  const char	*device_path_tmp;	/* Device object path */
  char		*device_path = NULL;	/* Device object path */


  message = dbus_message_new_method_call(COLORD_DBUS_SERVICE,
                                         COLORD_DBUS_PATH,
                                         COLORD_DBUS_INTERFACE,
                                         "FindDeviceById");

  dbus_message_iter_init_append(message, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &device_id);

 /*
  * Send the FindDeviceById request synchronously...
  */

  dbus_error_init(&error);
  cupsdLogMessage(CUPSD_LOG_DEBUG, "Calling FindDeviceById(%s)", device_id);
  reply = dbus_connection_send_with_reply_and_block(colord_con, message,
                                                    COLORD_DBUS_TIMEOUT,
                                                    &error);
  if (!reply)
  {
    cupsdLogMessage(CUPSD_LOG_DEBUG, "FindDeviceById failed: %s:%s",
		    error.name, error.message);
    dbus_error_free(&error);
    goto out;
  }

 /*
  * Get reply data...
  */

  dbus_message_iter_init(reply, &args);
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH)
  {
    cupsdLogMessage(CUPSD_LOG_WARN,
                    "FindDeviceById failed: Incorrect reply type.");
    goto out;
  }

  dbus_message_iter_get_basic(&args, &device_path_tmp);
  if (device_path_tmp)
    device_path = strdup(device_path_tmp);

out:

  if (message)
    dbus_message_unref(message);

  if (reply)
    dbus_message_unref(reply);

  return (device_path);
}


/*
 * 'colord_get_qualifier_format()' - Get the qualifier format.
 *
 * Note: Returns a value of "ColorSpace.MediaType.Resolution" by default.
 */

static void
colord_get_qualifier_format(
    ppd_file_t *ppd,			/* I - PPD file data */
    char       *format[3])		/* I - Format tuple */
{
  const char	*tmp;			/* Temporary string */
  ppd_attr_t	*attr;			/* Profile attributes */


 /*
  * Get 1st section...
  */

  if ((attr = ppdFindAttr(ppd, "cupsICCQualifier1", NULL)) != NULL)
    tmp = attr->value;
  else if (ppdFindAttr(ppd, "DefaultColorModel", NULL))
    tmp = "ColorModel";
  else if (ppdFindAttr(ppd, "DefaultColorSpace", NULL))
    tmp = "ColorSpace";
  else
    tmp = "";

  format[0] = strdup(tmp);

 /*
  * Get 2nd section...
  */

  if ((attr = ppdFindAttr(ppd, "cupsICCQualifier2", NULL)) != NULL)
    tmp = attr->value;
  else
    tmp = "MediaType";

  format[1] = strdup(tmp);

 /*
  * Get 3rd section...
  */

  if ((attr = ppdFindAttr(ppd, "cupsICCQualifier3", NULL)) != NULL)
    tmp = attr->value;
  else
    tmp = "Resolution";

  format[2] = strdup(tmp);
}


/*
 * 'colord_register_printer()' - Register profiles for a printer.
 */

static void
colord_register_printer(
    cupsd_printer_t *p)			/* I - printer */
{
  char		ppdfile[1024],		/* PPD filename */
		iccfile[1024];		/* ICC filename */
  ppd_file_t	*ppd;			/* PPD file */
  cups_array_t	*profiles;		/* Profile paths array */
  ppd_attr_t	*attr;			/* Profile attributes */
  const char	*device_colorspace;	/* Device colorspace */
  char		*format[3];		/* Qualifier format tuple */


 /*
  * Ensure we have a D-Bus connection...
  */

  if (!colord_con)
    return;

 /*
  * Try opening the PPD file for this printer...
  */

  snprintf(ppdfile, sizeof(ppdfile), "%s/ppd/%s.ppd", ServerRoot, p->name);
  if ((ppd = _ppdOpenFile(ppdfile, _PPD_LOCALIZATION_ICC_PROFILES)) == NULL)
    return;

 /*
  * Find out the qualifier format
  */

  colord_get_qualifier_format(ppd, format);

 /*
  * See if we have any embedded profiles...
  */

  profiles = cupsArrayNew3(NULL, NULL, NULL, 0, (cups_acopy_func_t)strdup,
			   (cups_afree_func_t)free);
  for (attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
       attr;
       attr = ppdFindNextAttr(ppd, "cupsICCProfile", NULL))
    if (attr->spec[0] && attr->value && attr->value[0])
    {
      if (attr->value[0] != '/')
        snprintf(iccfile, sizeof(iccfile), "%s/profiles/%s", DataDir,
                 attr->value);
      else
        strlcpy(iccfile, attr->value, sizeof(iccfile));

      if (_cupsFileCheck(iccfile, _CUPS_FILE_CHECK_FILE, !RunUser,
			 cupsdLogFCMessage, p))
	continue;

      colord_create_profile(profiles, p->name, attr->spec, COLORD_SPACE_UNKNOWN,
			    format, iccfile, COLORD_SCOPE_TEMP);
    }

 /*
  * Add the grayscale profile first.  We always have a grayscale profile.
  */

  colord_create_profile(profiles, p->name, "Gray..", COLORD_SPACE_GRAY,
                        format, NULL, COLORD_SCOPE_TEMP);

 /*
  * Then add the RGB/CMYK/DeviceN color profile...
  */

  device_colorspace = "unknown";
  switch (ppd->colorspace)
  {
    case PPD_CS_RGB :
    case PPD_CS_CMY :
        device_colorspace = COLORD_SPACE_RGB;
        colord_create_profile(profiles, p->name, "RGB..", COLORD_SPACE_RGB,
			      format, NULL, COLORD_SCOPE_TEMP);
        break;

    case PPD_CS_RGBK :
    case PPD_CS_CMYK :
        device_colorspace = COLORD_SPACE_CMYK;
        colord_create_profile(profiles, p->name, "CMYK..", COLORD_SPACE_CMYK,
                              format, NULL, COLORD_SCOPE_TEMP);
        break;

    case PPD_CS_GRAY :
        device_colorspace = COLORD_SPACE_GRAY;
        break;

    case PPD_CS_N :
        colord_create_profile(profiles, p->name, "DeviceN..",
                              COLORD_SPACE_UNKNOWN, format, NULL,
			      COLORD_SCOPE_TEMP);
        break;
  }

 /*
  * Register the device with colord.
  */

  cupsdLogMessage(CUPSD_LOG_INFO, "Registering ICC color profiles for \"%s\".",
                  p->name);
  colord_create_device(p, ppd, profiles, device_colorspace, format,
		       COLORD_RELATION_SOFT, COLORD_SCOPE_TEMP);

 /*
  * Free any memory we used...
  */

  cupsArrayDelete(profiles);

  free(format[0]);
  free(format[1]);
  free(format[2]);

  ppdClose(ppd);
}


/*
 * 'colord_unregister_printer()' - Unregister profiles for a printer.
 */

static void
colord_unregister_printer(
    cupsd_printer_t *p)			/* I - printer */
{
  char	device_id[1024];		/* Device ID as understood by colord */


 /*
  * Ensure we have a D-Bus connection...
  */

  if (!colord_con)
    return;

 /*
  * Just delete the device itself, and leave the profiles registered
  */

  snprintf(device_id, sizeof(device_id), "cups-%s", p->name);
  colord_delete_device(device_id);
}
#endif /* __APPLE__ */


/*
 * End of "$Id: colorman.c 12369 2014-12-15 14:51:28Z msweet $".
 */
