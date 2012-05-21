/*
 * "$Id$"
 *
 *   Color management routines for the CUPS scheduler.
 *
 *   Copyright 2007-2012 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
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
#endif /* __APPLE__ */


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
#endif /* __APPLE__ */


/*
 * 'cupsdRegisterColor()' - Register vendor color profiles in a PPD file.
 */

void
cupsdRegisterColor(cupsd_printer_t *p)	/* I - Printer */
{
#ifdef __APPLE__
  apple_unregister_profiles(p);
  apple_register_profiles(p);

#elif defined(HAVE_DBUS)
  /* colord stuff goes here */
#endif /* __APPLE__ */
}


/*
 * 'cupsdStartColor()' - Initialize color management.
 */

void
cupsdStartColor(void)
{
#if !defined(__APPLE__) && defined(HAVE_DBUS)
  /* colord stuff goes here */
#endif /* !__APPLE__ && HAVE_DBUS */
}


/*
 * 'cupsdStopColor()' - Shutdown color management.
 */

void
cupsdStopColor(void)
{
#if !defined(__APPLE__) && defined(HAVE_DBUS)
  /* colord stuff goes here */
#endif /* !__APPLE__ && HAVE_DBUS */
}


/*
 * 'cupsdUnregisterColor()' - Unregister vendor color profiles in a PPD file.
 */

void
cupsdUnregisterColor(cupsd_printer_t *p)/* I - Printer */
{
#ifdef __APPLE__
  apple_unregister_profiles(p);

#elif defined(HAVE_DBUS)
  /* colord stuff goes here */
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

 if (iccfile)
 {
    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
						  (const UInt8 *)iccfile,
                                                  strlen(iccfile), false);

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
  const char		*profile_key;	/* Profile keyword */
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

  if ((attr = ppdFindAttr(ppd, "APTiogaProfile", NULL)) != NULL)
    profile_key = "APTiogaProfile";
  else
  {
    attr = ppdFindAttr(ppd, "cupsICCProfile", NULL);
    profile_key = "cupsICCProfile";
  }

  for (num_profiles = 0; attr; attr = ppdFindNextAttr(ppd, profile_key, NULL))
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
    if (profile_key[0] == 'A')
    {
     /*
      * For Tioga PPDs, get the default profile using the DefaultAPTiogaProfile
      * attribute...
      */

      if ((attr = ppdFindAttr(ppd, "DefaultAPTiogaProfile", NULL)) != NULL &&
	  attr->value)
        default_profile_id = atoi(attr->value);

      q1_choice = q2_choice = q3_choice = NULL;
    }
    else
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
    }

   /*
    * Loop through the profiles listed in the PPD...
    */

    languages = _ppdGetLanguages(ppd);

    for (attr = ppdFindAttr(ppd, profile_key, NULL);
	 attr;
	 attr = ppdFindNextAttr(ppd, profile_key, NULL))
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
	  continue;

        if (profile_key[0] == 'c')
	{
	  cupsArraySave(ppd->sorted_attrs);

	  if ((profileid_attr = ppdFindAttr(ppd, "cupsProfileID",
					    attr->spec)) != NULL &&
	      profileid_attr->value && isdigit(profileid_attr->value[0] & 255))
	    profile_id = (unsigned)strtoul(profileid_attr->value, NULL, 10);
	  else
	    profile_id = _ppdHashName(attr->spec);

	  cupsArrayRestore(ppd->sorted_attrs);
        }
	else
	  profile_id = atoi(attr->spec);

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
   /*
    * Because we may have registered the printer profiles using a prior device
    * ID-based UUID, remove both the old style UUID and current UUID for the
    * printer.
    */

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


/* colord stuff goes here */
#endif /* __APPLE__ */


/*
 * End of "$Id$".
 */
