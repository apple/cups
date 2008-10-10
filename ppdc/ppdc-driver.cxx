//
// "$Id$"
//
//   PPD file compiler definitions for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
//   Copyright 2002-2006 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   ppdcDriver::ppdcDriver()       - Create a new printer driver.
//   ppdcDriver::~ppdcDriver()      - Destroy a printer driver.
//   ppdcDriver::find_attr()        - Find an attribute.
//   ppdcDriver::find_group()       - Find a group.
//   ppdcDriver::find_option()      - Find an option.
//   ppdcDriver::set_default_size() - Set the default size name.
//   ppdcDriver::set_file_name()    - Set the full filename.
//   ppdcDriver::set_manufacturer() - Set the manufacturer name.
//   ppdcDriver::set_model_name()   - Set the model name.
//   ppdcDriver::set_pc_file_name() - Set the PC filename.
//   ppdcDriver::set_version()      - Set the version string.
//   ppdcDriver::write_ppd_file()   - Write a PPD file...
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <cups/globals.h>


//
// 'ppdcDriver::ppdcDriver()' - Create a new printer driver.
//

ppdcDriver::ppdcDriver(ppdcDriver *d)	// I - Printer driver template
{
  ppdcGroup	*g;			// Current group


  if (d)
  {
    // Bump the use count of any strings we inherit...
    if (d->manufacturer)
      d->manufacturer->get();
    if (d->version)
      d->version->get();
    if (d->default_font)
      d->default_font->get();
    if (d->default_size)
      d->default_size->get();
    if (d->custom_size_code)
      d->custom_size_code->get();

    // Copy all of the data from the driver template...
    copyright           = new ppdcArray(d->copyright);
    manufacturer        = d->manufacturer;
    model_name          = 0;
    file_name           = 0;
    pc_file_name        = 0;
    type                = d->type;
    version             = d->version;
    model_number        = d->model_number;
    manual_copies       = d->manual_copies;
    color_device        = d->color_device;
    throughput          = d->throughput;
    attrs               = new ppdcArray(d->attrs);
    constraints         = new ppdcArray(d->constraints);
    filters             = new ppdcArray(d->filters);
    fonts               = new ppdcArray(d->fonts);
    profiles            = new ppdcArray(d->profiles);
    sizes               = new ppdcArray(d->sizes);
    default_font        = d->default_font;
    default_size        = d->default_size;
    variable_paper_size = d->variable_paper_size;
    custom_size_code    = d->custom_size_code;
    left_margin         = d->left_margin;
    bottom_margin       = d->bottom_margin;
    right_margin        = d->right_margin;
    top_margin          = d->top_margin;
    max_width           = d->max_width;
    max_length          = d->max_length;
    min_width           = d->min_width;
    min_length          = d->min_length;

    // Then copy the groups manually, since we want separate copies
    // of the groups and options...
    groups = new ppdcArray();

    for (g = (ppdcGroup *)d->groups->first(); g; g = (ppdcGroup *)d->groups->next())
      groups->add(new ppdcGroup(g));
  }
  else
  {
    // Zero all of the data in the driver...
    copyright           = new ppdcArray();
    manufacturer        = 0;
    model_name          = 0;
    file_name           = 0;
    pc_file_name        = 0;
    version             = 0;
    type                = PPDC_DRIVER_CUSTOM;
    model_number        = 0;
    manual_copies       = 0;
    color_device        = 0;
    throughput          = 1;
    attrs               = new ppdcArray();
    constraints         = new ppdcArray();
    fonts               = new ppdcArray();
    filters             = new ppdcArray();
    groups              = new ppdcArray();
    profiles            = new ppdcArray();
    sizes               = new ppdcArray();
    default_font        = 0;
    default_size        = 0;
    variable_paper_size = 0;
    custom_size_code    = 0;
    left_margin         = 0;
    bottom_margin       = 0;
    right_margin        = 0;
    top_margin          = 0;
    max_width           = 0;
    max_length          = 0;
    min_width           = 0;
    min_length          = 0;
  }
}


//
// 'ppdcDriver::~ppdcDriver()' - Destroy a printer driver.
//

ppdcDriver::~ppdcDriver()
{
  delete copyright;

  if (manufacturer)
    manufacturer->release();
  if (model_name)
    model_name->release();
  if (file_name)
    file_name->release();
  if (pc_file_name)
    pc_file_name->release();
  if (version)
    version->release();
  if (default_font)
    default_font->release();
  if (default_size)
    default_size->release();
  if (custom_size_code)
    custom_size_code->release();

  delete attrs;
  delete constraints;
  delete filters;
  delete fonts;
  delete groups;
  delete profiles;
  delete sizes;
}


//
// 'ppdcDriver::find_attr()' - Find an attribute.
//

ppdcAttr *				// O - Attribute or NULL
ppdcDriver::find_attr(const char *k,	// I - Keyword string
                      const char *s)	// I - Spec string
{
  ppdcAttr	*a;			// Current attribute


  for (a = (ppdcAttr *)attrs->first(); a; a = (ppdcAttr *)attrs->next())
    if (!strcmp(a->name->value, k) &&
        ((!s && (!a->selector->value || !a->selector->value[0])) ||
	 (!s && !a->selector->value && !strcmp(a->selector->value, s))))
      return (a);

  return (NULL);
}


//
// 'ppdcDriver::find_group()' - Find a group.
//

ppdcGroup *				// O - Matching group or NULL
ppdcDriver::find_group(const char *n)	// I - Group name
{
  ppdcGroup	*g;			// Current group


  for (g = (ppdcGroup *)groups->first(); g; g = (ppdcGroup *)groups->next())
    if (!strcasecmp(n, g->name->value))
      return (g);

  return (0);
}


//
// 'ppdcDriver::find_option()' - Find an option.
//

ppdcOption *				// O - Matching option or NULL
ppdcDriver::find_option(const char *n)	// I - Option name
{
  ppdcGroup	*g;			// Current group
  ppdcOption	*o;			// Current option


  for (g = (ppdcGroup *)groups->first(); g; g = (ppdcGroup *)groups->next())
    for (o = (ppdcOption *)g->options->first(); o; o = (ppdcOption *)g->options->next())
      if (!strcasecmp(n, o->name->value))
        return (o);

  return (0);
}


//
// 'ppdcDriver::set_custom_size_code()' - Set the custom page size code.
//

void
ppdcDriver::set_custom_size_code(
    const char *c)			// I - CustomPageSize code
{
  if (custom_size_code)
    custom_size_code->release();

  custom_size_code = new ppdcString(c);
}


//
// 'ppdcDriver::set_default_font()' - Set the default font name.
//

void
ppdcDriver::set_default_font(
    ppdcFont *f)			// I - Font
{
  if (default_font)
    default_font->release();

  if (f)
  {
    f->name->get();
    default_font = f->name;
  }
  else
    default_font = 0;
}


//
// 'ppdcDriver::set_default_size()' - Set the default size name.
//

void
ppdcDriver::set_default_size(
    ppdcMediaSize *m)			// I - Media size
{
  if (default_size)
    default_size->release();

  if (m)
  {
    m->name->get();
    default_size = m->name;
  }
  else
    default_size = 0;
}


//
// 'ppdcDriver::set_file_name()' - Set the full filename.
//

void
ppdcDriver::set_file_name(const char *f)// I - Filename
{
  if (file_name)
    file_name->release();

  file_name = new ppdcString(f);
}


//
// 'ppdcDriver::set_manufacturer()' - Set the manufacturer name.
//

void
ppdcDriver::set_manufacturer(
    const char *m)			// I - Model name
{
  if (manufacturer)
    manufacturer->release();

  manufacturer = new ppdcString(m);
}


//
// 'ppdcDriver::set_model_name()' - Set the model name.
//

void
ppdcDriver::set_model_name(
    const char *m)			// I - Model name
{
  if (model_name)
    model_name->release();

  model_name = new ppdcString(m);
}


//
// 'ppdcDriver::set_pc_file_name()' - Set the PC filename.
//

void
ppdcDriver::set_pc_file_name(
    const char *f)			// I - Filename
{
  if (pc_file_name)
    pc_file_name->release();

  pc_file_name = new ppdcString(f);
}


//
// 'ppdcDriver::set_version()' - Set the version string.
//

void
ppdcDriver::set_version(const char *v)	// I - Version
{
  if (version)
    version->release();

  version = new ppdcString(v);
}


//
// 'ppdcDriver::write_ppd_file()' - Write a PPD file...
//

int					// O - 0 on success, -1 on failure
ppdcDriver::write_ppd_file(
    cups_file_t    *fp,			// I - PPD file
    ppdcCatalog    *catalog,		// I - Message catalog
    ppdcArray      *locales,		// I - Additional languages to add
    ppdcSource     *src,		// I - Driver source
    ppdcLineEnding le)			// I - Line endings to use
{
  bool			delete_cat;	// Delete the catalog when we are done?
  char			query[42];	// Query attribute
  ppdcString		*s;		// Copyright string
  ppdcGroup		*g;		// Current group
  ppdcOption		*o;		// Current option
  ppdcChoice		*c;		// Current choice
  ppdcMediaSize		*m;		// Current media size
  ppdcProfile		*p;		// Current color profile
  ppdcFilter		*f;		// Current filter
  ppdcFont		*fn,		// Current font
			*bfn;		// Current base font
  ppdcConstraint	*cn;		// Current constraint
  ppdcAttr		*a;		// Current attribute
  const char		*lf;		// Linefeed character to use


  // If we don't have a message catalog, use an empty (English) one...
  if (!catalog)
  {
    catalog    = new ppdcCatalog("en");
    delete_cat = true;
  }
  else
    delete_cat = false;

  // Figure out the end-of-line string...
  if (le == PPDC_LFONLY)
    lf = "\n";
  else if (le == PPDC_CRONLY)
    lf = "\r";
  else
    lf = "\r\n";

  // Write the standard header stuff...
  cupsFilePrintf(fp, "*PPD-Adobe: \"4.3\"%s", lf);
  cupsFilePrintf(fp, "*%% PPD file for %s with CUPS.%s", model_name->value, lf);
  cupsFilePrintf(fp,
                 "*%% Created by the CUPS PPD Compiler " CUPS_SVERSION ".%s",
		 lf);
  for (s = (ppdcString *)copyright->first();
       s;
       s = (ppdcString *)copyright->next())
    cupsFilePrintf(fp, "*%% %s%s", catalog->find_message(s->value), lf);
  cupsFilePrintf(fp, "*FormatVersion: \"4.3\"%s", lf);
  cupsFilePrintf(fp, "*FileVersion: \"%s\"%s", version->value, lf);

  a = find_attr("LanguageVersion", NULL);
  cupsFilePrintf(fp, "*LanguageVersion: %s%s",
        	 catalog->find_message(a ? a->value->value : "English"), lf);

  a = find_attr("LanguageEncoding", NULL);
  cupsFilePrintf(fp, "*LanguageEncoding: %s%s",
        	 catalog->find_message(a ? a->value->value : "ISOLatin1"), lf);

  cupsFilePrintf(fp, "*PCFileName: \"%s\"%s", pc_file_name->value, lf);

  for (a = (ppdcAttr *)attrs->first(); a; a = (ppdcAttr *)attrs->next())
    if (!strcmp(a->name->value, "Product"))
      break;

  if (a)
  {
    for (; a; a = (ppdcAttr *)attrs->next())
      if (!strcmp(a->name->value, "Product"))
	cupsFilePrintf(fp, "*Product: \"%s\"%s", a->value->value, lf);
  }
  else
    cupsFilePrintf(fp, "*Product: \"(%s)\"%s", model_name->value, lf);

  cupsFilePrintf(fp, "*Manufacturer: \"%s\"%s",
        	 catalog->find_message(manufacturer->value), lf);

  if ((a = find_attr("ModelName", NULL)) != NULL)
    cupsFilePrintf(fp, "*ModelName: \"%s\"%s",
        	   catalog->find_message(a->value->value), lf);
  else if (strncasecmp(model_name->value, manufacturer->value,
                       strlen(manufacturer->value)))
    cupsFilePrintf(fp, "*ModelName: \"%s %s\"%s",
        	   catalog->find_message(manufacturer->value),
        	   catalog->find_message(model_name->value), lf);
  else
    cupsFilePrintf(fp, "*ModelName: \"%s\"%s",
        	   catalog->find_message(model_name->value), lf);

  if ((a = find_attr("ShortNickName", NULL)) != NULL)
    cupsFilePrintf(fp, "*ShortNickName: \"%s\"%s",
        	   catalog->find_message(a->value->value), lf);
  else if (strncasecmp(model_name->value, manufacturer->value,
                       strlen(manufacturer->value)))
    cupsFilePrintf(fp, "*ShortNickName: \"%s %s\"%s",
        	   catalog->find_message(manufacturer->value),
        	   catalog->find_message(model_name->value), lf);
  else
    cupsFilePrintf(fp, "*ShortNickName: \"%s\"%s",
        	   catalog->find_message(model_name->value), lf);

  if ((a = find_attr("NickName", NULL)) != NULL)
    cupsFilePrintf(fp, "*NickName: \"%s\"%s",
        	   catalog->find_message(a->value->value), lf);
  else if (strncasecmp(model_name->value, manufacturer->value,
                       strlen(manufacturer->value)))
    cupsFilePrintf(fp, "*NickName: \"%s %s, %s\"%s",
        	   catalog->find_message(manufacturer->value),
        	   catalog->find_message(model_name->value), version->value,
		   lf);
  else
    cupsFilePrintf(fp, "*NickName: \"%s, %s\"%s",
        	   catalog->find_message(model_name->value), version->value,
		   lf);

  for (a = (ppdcAttr *)attrs->first(); a; a = (ppdcAttr *)attrs->next())
    if (!strcmp(a->name->value, "PSVersion"))
      break;

  if (a)
  {
    for (; a; a = (ppdcAttr *)attrs->next())
      if (!strcmp(a->name->value, "PSVersion"))
	cupsFilePrintf(fp, "*PSVersion: \"%s\"%s", a->value->value, lf);
  }
  else
    cupsFilePrintf(fp, "*PSVersion: \"(3010.000) 0\"%s", lf);

  if ((a = find_attr("LanguageLevel", NULL)) != NULL)
    cupsFilePrintf(fp, "*LanguageLevel: \"%s\"%s", a->value->value, lf);
  else
    cupsFilePrintf(fp, "*LanguageLevel: \"3\"%s", lf);

  cupsFilePrintf(fp, "*ColorDevice: %s%s", color_device ? "True" : "False", lf);

  if ((a = find_attr("DefaultColorSpace", NULL)) != NULL)
    cupsFilePrintf(fp, "*DefaultColorSpace: %s%s", a->value->value, lf);
  else
    cupsFilePrintf(fp, "*DefaultColorSpace: %s%s",
                   color_device ? "RGB" : "Gray", lf);

  if ((a = find_attr("FileSystem", NULL)) != NULL)
    cupsFilePrintf(fp, "*FileSystem: %s%s", a->value->value, lf);
  else
    cupsFilePrintf(fp, "*FileSystem: False%s", lf);

  cupsFilePrintf(fp, "*Throughput: \"%d\"%s", throughput, lf);

  if ((a = find_attr("LandscapeOrientation", NULL)) != NULL)
    cupsFilePrintf(fp, "*LandscapeOrientation: %s%s", a->value->value, lf);
  else
    cupsFilePrintf(fp, "*LandscapeOrientation: Plus90%s", lf);

  if ((a = find_attr("TTRasterizer", NULL)) != NULL)
    cupsFilePrintf(fp, "*TTRasterizer: %s%s", a->value->value, lf);
  else if (type != PPDC_DRIVER_PS)
    cupsFilePrintf(fp, "*TTRasterizer: Type42%s", lf);

  if (attrs->count)
  {
    // Write driver-defined attributes...
    cupsFilePrintf(fp, "*%% Driver-defined attributes...%s", lf);
    for (a = (ppdcAttr *)attrs->first(); a; a = (ppdcAttr *)attrs->next())
    {
      if (!strcmp(a->name->value, "Product") ||
          !strcmp(a->name->value, "PSVersion") ||
          !strcmp(a->name->value, "LanguageLevel") ||
          !strcmp(a->name->value, "DefaultColorSpace") ||
          !strcmp(a->name->value, "FileSystem") ||
          !strcmp(a->name->value, "LandscapeOrientation") ||
          !strcmp(a->name->value, "TTRasterizer") ||
          !strcmp(a->name->value, "LanguageVersion") ||
          !strcmp(a->name->value, "LanguageEncoding") ||
          !strcmp(a->name->value, "ModelName") ||
          !strcmp(a->name->value, "NickName") ||
          !strcmp(a->name->value, "ShortNickName") ||
	  !strcmp(a->name->value, "cupsVersion"))
	continue;

      if (a->name->value[0] == '?' &&
          (find_option(a->name->value + 1) ||
	   !strcmp(a->name->value, "?ImageableArea") ||
	   !strcmp(a->name->value, "?PageRegion") ||
	   !strcmp(a->name->value, "?PageSize") ||
	   !strcmp(a->name->value, "?PaperDimension")))
        continue;

      if (!a->selector->value || !a->selector->value[0])
	cupsFilePrintf(fp, "*%s", a->name->value);
      else if (!a->text->value || !a->text->value[0])
	cupsFilePrintf(fp, "*%s %s", a->name->value, a->selector->value);
      else
	cupsFilePrintf(fp, "*%s %s/%s", a->name->value, a->selector->value,
        	       a->text->value);

      if (strcmp(a->value->value, "False") &&
          strcmp(a->value->value, "True") &&
	  strcmp(a->name->value, "1284Modes") &&
	  strcmp(a->name->value, "InkName") &&
	  strcmp(a->name->value, "PageStackOrder") &&
	  strncmp(a->name->value, "ParamCustom", 11) &&
	  strcmp(a->name->value, "Protocols") &&
	  strcmp(a->name->value, "ReferencePunch") &&
	  strncmp(a->name->value, "Default", 7))
      {
	cupsFilePrintf(fp, ": \"%s\"%s", a->value->value, lf);

	if (strchr(a->value->value, '\n'))
          cupsFilePrintf(fp, "*End%s", lf);
      }
      else
	cupsFilePrintf(fp, ": %s%s", a->value->value, lf);
    }
  }

  if (type != PPDC_DRIVER_PS || filters->count)
  {
    if ((a = find_attr("cupsVersion", NULL)) != NULL)
      cupsFilePrintf(fp, "*cupsVersion: %s%s", a->value->value, lf);
    else
      cupsFilePrintf(fp, "*cupsVersion: %d.%d%s", CUPS_VERSION_MAJOR,
		     CUPS_VERSION_MINOR, lf);
    cupsFilePrintf(fp, "*cupsModelNumber: %d%s", model_number, lf);
    cupsFilePrintf(fp, "*cupsManualCopies: %s%s",
                   manual_copies ? "True" : "False", lf);

    if (filters->count)
    {
      for (f = (ppdcFilter *)filters->first();
           f;
	   f = (ppdcFilter *)filters->next())
	cupsFilePrintf(fp, "*cupsFilter: \"%s %d %s\"%s", f->mime_type->value,
	               f->cost, f->program->value, lf);
    }
    else
    {
      switch (type)
      {
        case PPDC_DRIVER_LABEL :
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-raster 50 "
	        	     "rastertolabel\"%s", lf);
	    break;

        case PPDC_DRIVER_EPSON :
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-raster 50 "
	        	     "rastertoepson\"%s", lf);
	    break;

        case PPDC_DRIVER_ESCP :
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-command 50 "
	        	     "commandtoescpx\"%s", lf);
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-raster 50 "
	        	     "rastertoescpx\"%s", lf);
	    break;

        case PPDC_DRIVER_HP :
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-raster 50 "
	        	     "rastertohp\"%s", lf);
	    break;

        case PPDC_DRIVER_PCL :
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-command 50 "
	        	     "commandtopclx\"%s", lf);
	    cupsFilePrintf(fp, "*cupsFilter: \"application/vnd.cups-raster 50 "
	        	     "rastertopclx\"%s", lf);
	    break;

	default :
	    break;
      }
    }

    for (p = (ppdcProfile *)profiles->first();
         p;
	 p = (ppdcProfile *)profiles->next())
      cupsFilePrintf(fp,
                     "*cupsColorProfile %s/%s: \"%.3f %.3f %.3f %.3f %.3f %.3f "
		     "%.3f %.3f %.3f %.3f %.3f\"%s",
		     p->resolution->value, p->media_type->value,
		     p->density, p->gamma,
		     p->profile[0], p->profile[1],
		     p->profile[2], p->profile[3],
		     p->profile[4], p->profile[5],
		     p->profile[6], p->profile[7],
		     p->profile[8], lf);
  }

  if (locales)
  {
    // Add localizations for additional languages...
    ppdcString	*locale;		// Locale name
    ppdcCatalog	*locatalog;		// Message catalog for locale


    // Write the list of languages...
    cupsFilePrintf(fp, "*cupsLanguages: \"en");

    for (locale = (ppdcString *)locales->first();
         locale;
	 locale = (ppdcString *)locales->next())
    {
      // Skip (US) English...
      if (!strcmp(locale->value, "en") || !strcmp(locale->value, "en_US"))
        continue;

      // See if we have a po file for this language...
      if (!src->find_po(locale->value))
      {
        // No, see if we can use the base file?
        locatalog = new ppdcCatalog(locale->value);

	if (locatalog->messages->count == 0)
	{
	  // No, skip this one...
          _cupsLangPrintf(stderr,
	                  _("ppdc: No message catalog provided for locale "
			    "%s!\n"), locale->value);
          continue;
	}

        // Add the base file to the list...
	src->po_files->add(locatalog);
      }

      cupsFilePrintf(fp, " %s", locale->value);
    }

    cupsFilePrintf(fp, "\"%s", lf);
  }

  for (cn = (ppdcConstraint *)constraints->first();
       cn;
       cn = (ppdcConstraint *)constraints->next())
  {
    // First constrain 1 against 2...
    if (!strncmp(cn->option1->value, "*Custom", 7) ||
        !strncmp(cn->option2->value, "*Custom", 7))
      cupsFilePuts(fp, "*NonUIConstraints: ");
    else
      cupsFilePuts(fp, "*UIConstraints: ");

    if (cn->option1->value[0] != '*')
      cupsFilePutChar(fp, '*');

    cupsFilePuts(fp, cn->option1->value);

    if (cn->choice1->value)
      cupsFilePrintf(fp, " %s", cn->choice1->value);

    cupsFilePutChar(fp, ' ');

    if (cn->option2->value[0] != '*')
      cupsFilePutChar(fp, '*');

    cupsFilePuts(fp, cn->option2->value);

    if (cn->choice2->value)
      cupsFilePrintf(fp, " %s", cn->choice2->value);

    cupsFilePuts(fp, lf);

    // Then constrain 2 against 1...
    if (!strncmp(cn->option1->value, "*Custom", 7) ||
        !strncmp(cn->option2->value, "*Custom", 7))
      cupsFilePuts(fp, "*NonUIConstraints: ");
    else
      cupsFilePuts(fp, "*UIConstraints: ");

    if (cn->option2->value[0] != '*')
      cupsFilePutChar(fp, '*');

    cupsFilePuts(fp, cn->option2->value);

    if (cn->choice2->value)
      cupsFilePrintf(fp, " %s", cn->choice2->value);

    cupsFilePutChar(fp, ' ');

    if (cn->option1->value[0] != '*')
      cupsFilePutChar(fp, '*');

    cupsFilePuts(fp, cn->option1->value);

    if (cn->choice1->value)
      cupsFilePrintf(fp, " %s", cn->choice1->value);

    cupsFilePuts(fp, lf);
  }

  // PageSize option...
  cupsFilePrintf(fp, "*OpenUI *PageSize/Media Size: PickOne%s", lf);
  cupsFilePrintf(fp, "*OrderDependency: 10 AnySetup *PageSize%s", lf);
  cupsFilePrintf(fp, "*DefaultPageSize: %s%s",
                 default_size ? default_size->value : "Letter", lf);

  for (m = (ppdcMediaSize *)sizes->first();
       m;
       m = (ppdcMediaSize *)sizes->next())
    if (m->size_code->value)
    {
      cupsFilePrintf(fp, "*PageSize %s/%s: \"%s\"%s",
        	     m->name->value, catalog->find_message(m->text->value),
		     m->size_code->value, lf);

      if (strchr(m->size_code->value, '\n') ||
          strchr(m->size_code->value, '\r'))
        cupsFilePrintf(fp, "*End%s", lf);
    }
    else
      cupsFilePrintf(fp,
                     "*PageSize %s/%s: \"<</PageSize[%.0f %.0f]"
		     "/ImagingBBox null>>setpagedevice\"%s",
        	     m->name->value, catalog->find_message(m->text->value),
		     m->width, m->length, lf);

  if ((a = find_attr("?PageSize", NULL)) != NULL)
  {
    cupsFilePrintf(fp, "*?PageSize: \"%s\"%s", a->value->value, lf);

    if (strchr(a->value->value, '\n') ||
        strchr(a->value->value, '\r'))
      cupsFilePrintf(fp, "*End%s", lf);
  }

  cupsFilePrintf(fp, "*CloseUI: *PageSize%s", lf);

  // PageRegion option...
  cupsFilePrintf(fp, "*OpenUI *PageRegion/Media Size: PickOne%s", lf);
  cupsFilePrintf(fp, "*OrderDependency: 10 AnySetup *PageRegion%s", lf);
  cupsFilePrintf(fp, "*DefaultPageRegion: %s%s",
                 default_size ? default_size->value : "Letter", lf);

  for (m = (ppdcMediaSize *)sizes->first();
       m;
       m = (ppdcMediaSize *)sizes->next())
    if (m->region_code->value)
    {
      cupsFilePrintf(fp, "*PageRegion %s/%s: \"%s\"%s",
        	     m->name->value, catalog->find_message(m->text->value),
		     m->region_code->value, lf);

      if (strchr(m->region_code->value, '\n') ||
          strchr(m->region_code->value, '\r'))
        cupsFilePrintf(fp, "*End%s", lf);
    }
    else
      cupsFilePrintf(fp,
                     "*PageRegion %s/%s: \"<</PageSize[%.0f %.0f]"
		     "/ImagingBBox null>>setpagedevice\"%s",
        	     m->name->value, catalog->find_message(m->text->value),
		     m->width, m->length, lf);

  if ((a = find_attr("?PageRegion", NULL)) != NULL)
  {
    cupsFilePrintf(fp, "*?PageRegion: \"%s\"%s", a->value->value, lf);

    if (strchr(a->value->value, '\n') ||
        strchr(a->value->value, '\r'))
      cupsFilePrintf(fp, "*End%s", lf);
  }

  cupsFilePrintf(fp, "*CloseUI: *PageRegion%s", lf);

  // ImageableArea info...
  cupsFilePrintf(fp, "*DefaultImageableArea: %s%s",
                 default_size ? default_size->value : "Letter", lf);

  for (m = (ppdcMediaSize *)sizes->first();
       m;
       m = (ppdcMediaSize *)sizes->next())
    cupsFilePrintf(fp, "*ImageableArea %s/%s: \"%.2f %.2f %.2f %.2f\"%s",
                   m->name->value, catalog->find_message(m->text->value),
	           m->left, m->bottom, m->width - m->right, m->length - m->top,
		   lf);

  if ((a = find_attr("?ImageableArea", NULL)) != NULL)
  {
    cupsFilePrintf(fp, "*?ImageableArea: \"%s\"%s", a->value->value, lf);

    if (strchr(a->value->value, '\n') ||
        strchr(a->value->value, '\r'))
      cupsFilePrintf(fp, "*End%s", lf);
  }

  // PaperDimension info...
  cupsFilePrintf(fp, "*DefaultPaperDimension: %s%s",
                 default_size ? default_size->value : "Letter", lf);

  for (m = (ppdcMediaSize *)sizes->first();
       m;
       m = (ppdcMediaSize *)sizes->next())
    cupsFilePrintf(fp, "*PaperDimension %s/%s: \"%.2f %.2f\"%s",
                   m->name->value, catalog->find_message(m->text->value),
	           m->width, m->length, lf);

  if ((a = find_attr("?PaperDimension", NULL)) != NULL)
  {
    cupsFilePrintf(fp, "*?PaperDimension: \"%s\"%s", a->value->value, lf);

    if (strchr(a->value->value, '\n') ||
        strchr(a->value->value, '\r'))
      cupsFilePrintf(fp, "*End%s", lf);
  }

  // Custom size support...
  if (variable_paper_size)
  {
    cupsFilePrintf(fp, "*MaxMediaWidth: \"%.2f\"%s", max_width, lf);
    cupsFilePrintf(fp, "*MaxMediaHeight: \"%.2f\"%s", max_length, lf);
    cupsFilePrintf(fp, "*HWMargins: %.2f %.2f %.2f %.2f\n",
	           left_margin, bottom_margin, right_margin, top_margin);

    if (custom_size_code && custom_size_code->value)
    {
      cupsFilePrintf(fp, "*CustomPageSize True: \"%s\"%s",
                     custom_size_code->value, lf);

      if (strchr(custom_size_code->value, '\n') ||
          strchr(custom_size_code->value, '\r'))
        cupsFilePrintf(fp, "*End%s", lf);
    }
    else
      cupsFilePrintf(fp,
		     "*CustomPageSize True: \"pop pop pop <</PageSize[5 -2 roll]"
		     "/ImagingBBox null>>setpagedevice\"%s", lf);

    if ((a = find_attr("ParamCustomPageSize", "Width")) != NULL)
      cupsFilePrintf(fp, "*ParamCustomPageSize Width: %s%s", a->value->value,
		     lf);
    else
      cupsFilePrintf(fp, "*ParamCustomPageSize Width: 1 points %.2f %.2f%s",
                     min_width, max_width, lf);

    if ((a = find_attr("ParamCustomPageSize", "Height")) != NULL)
      cupsFilePrintf(fp, "*ParamCustomPageSize Height: %s%s", a->value->value,
		     lf);
    else
      cupsFilePrintf(fp, "*ParamCustomPageSize Height: 2 points %.2f %.2f%s",
                     min_length, max_length, lf);

    if ((a = find_attr("ParamCustomPageSize", "WidthOffset")) != NULL)
      cupsFilePrintf(fp, "*ParamCustomPageSize WidthOffset: %s%s",
                     a->value->value, lf);
    else
      cupsFilePrintf(fp, "*ParamCustomPageSize WidthOffset: 3 points 0 0%s", lf);

    if ((a = find_attr("ParamCustomPageSize", "HeightOffset")) != NULL)
      cupsFilePrintf(fp, "*ParamCustomPageSize HeightOffset: %s%s",
                     a->value->value, lf);
    else
      cupsFilePrintf(fp, "*ParamCustomPageSize HeightOffset: 4 points 0 0%s", lf);

    if ((a = find_attr("ParamCustomPageSize", "Orientation")) != NULL)
      cupsFilePrintf(fp, "*ParamCustomPageSize Orientation: %s%s",
                     a->value->value, lf);
    else
      cupsFilePrintf(fp, "*ParamCustomPageSize Orientation: 5 int 0 0%s", lf);
  }

  if (type != PPDC_DRIVER_PS && !find_attr("RequiresPageRegion", NULL))
    cupsFilePrintf(fp, "*RequiresPageRegion All: True%s", lf);

  // All other options...
  for (g = (ppdcGroup *)groups->first(); g; g = (ppdcGroup *)groups->next())
  {
    if (!g->options->count)
      continue;

    if (strcasecmp(g->name->value, "General"))
      cupsFilePrintf(fp, "*OpenGroup: %s/%s%s", g->name->value,
                     catalog->find_message(g->text->value), lf);

    for (o = (ppdcOption *)g->options->first();
         o;
	 o = (ppdcOption *)g->options->next())
    {
      if (!o->choices->count)
        continue;

      if (!o->text->value || !strcmp(o->name->value, o->text->value))
	cupsFilePrintf(fp, "*OpenUI *%s: ", o->name->value);
      else
	cupsFilePrintf(fp, "*OpenUI *%s/%s: ", o->name->value,
	               catalog->find_message(o->text->value));

      switch (o->type)
      {
        case PPDC_BOOLEAN :
	    cupsFilePrintf(fp, "Boolean%s", lf);
	    break;
        default :
	    cupsFilePrintf(fp, "PickOne%s", lf);
	    break;
        case PPDC_PICKMANY :
	    cupsFilePrintf(fp, "PickMany%s", lf);
	    break;
      }

      cupsFilePrintf(fp, "*OrderDependency: %.1f ", o->order);
      switch (o->section)
      {
        default :
	    cupsFilePrintf(fp, "AnySetup");
	    break;
        case PPDC_SECTION_DOCUMENT :
	    cupsFilePrintf(fp, "DocumentSetup");
	    break;
        case PPDC_SECTION_EXIT :
	    cupsFilePrintf(fp, "ExitServer");
	    break;
        case PPDC_SECTION_JCL :
	    cupsFilePrintf(fp, "JCLSetup");
	    break;
        case PPDC_SECTION_PAGE :
	    cupsFilePrintf(fp, "PageSetup");
	    break;
        case PPDC_SECTION_PROLOG :
	    cupsFilePrintf(fp, "Prolog");
	    break;
      }

      cupsFilePrintf(fp, " *%s%s", o->name->value, lf);

      if (o->defchoice)
      {
        // Use the programmer-supplied default...
        cupsFilePrintf(fp, "*Default%s: %s%s", o->name->value,
	               o->defchoice->value, lf);
      }
      else
      {
        // Make the first choice the default...
        c = (ppdcChoice *)o->choices->first();
        cupsFilePrintf(fp, "*Default%s: %s%s", o->name->value, c->name->value,
		       lf);
      }

      for (c = (ppdcChoice *)o->choices->first();
           c;
	   c = (ppdcChoice *)o->choices->next())
      {
        // Write this choice...
	if (!c->text->value || !strcmp(c->name->value, c->text->value))
          cupsFilePrintf(fp, "*%s %s: \"%s\"%s", o->name->value, c->name->value,
	        	 c->code->value, lf);
        else
          cupsFilePrintf(fp, "*%s %s/%s: \"%s\"%s", o->name->value,
	                 c->name->value, catalog->find_message(c->text->value),
			 c->code->value, lf);

	// Multi-line commands need a *End line to terminate them.
        if (strchr(c->code->value, '\n') ||
	    strchr(c->code->value, '\r'))
	  cupsFilePrintf(fp, "*End%s", lf);
      }

      snprintf(query, sizeof(query), "?%s", o->name->value);

      if ((a = find_attr(query, NULL)) != NULL)
      {
	cupsFilePrintf(fp, "*%s: \"%s\"\n", query, a->value->value);

	if (strchr(a->value->value, '\n') ||
            strchr(a->value->value, '\r'))
	  cupsFilePrintf(fp, "*End%s", lf);
      }

      cupsFilePrintf(fp, "*CloseUI: *%s%s", o->name->value, lf);
    }

    if (strcasecmp(g->name->value, "General"))
      cupsFilePrintf(fp, "*CloseGroup: %s%s", g->name->value, lf);
  }

  if (locales)
  {
    // Add localizations for additional languages...
    ppdcString	*locale;		// Locale name
    ppdcCatalog	*locatalog;		// Message catalog for locale


    // Write the translation strings for each language...
    for (locale = (ppdcString *)locales->first();
         locale;
	 locale = (ppdcString *)locales->next())
    {
      // Skip (US) English...
      if (!strcmp(locale->value, "en") || !strcmp(locale->value, "en_US"))
        continue;

      // Skip missing languages...
      if ((locatalog = src->find_po(locale->value)) == NULL)
        continue;

      // Do the core stuff first...
      cupsFilePrintf(fp, "*%s.Translation Manufacturer/%s: \"\"%s",
                     locale->value,
        	     locatalog->find_message(manufacturer->value), lf);

      if ((a = find_attr("ModelName", NULL)) != NULL)
	cupsFilePrintf(fp, "*%s.Translation ModelName/%s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(a->value->value), lf);
      else if (strncasecmp(model_name->value, manufacturer->value,
                	   strlen(manufacturer->value)))
	cupsFilePrintf(fp, "*%s.Translation ModelName/%s %s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(manufacturer->value),
        	       locatalog->find_message(model_name->value), lf);
      else
	cupsFilePrintf(fp, "*%s.Translation ModelName/%s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(model_name->value), lf);

      if ((a = find_attr("ShortNickName", NULL)) != NULL)
	cupsFilePrintf(fp, "*%s.Translation ShortNickName/%s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(a->value->value), lf);
      else if (strncasecmp(model_name->value, manufacturer->value,
                	   strlen(manufacturer->value)))
	cupsFilePrintf(fp, "*%s.Translation ShortNickName/%s %s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(manufacturer->value),
        	       locatalog->find_message(model_name->value), lf);
      else
	cupsFilePrintf(fp, "*%s.Translation ShortNickName/%s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(model_name->value), lf);

      if ((a = find_attr("NickName", NULL)) != NULL)
	cupsFilePrintf(fp, "*%s.Translation NickName/%s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(a->value->value), lf);
      else if (strncasecmp(model_name->value, manufacturer->value,
                	   strlen(manufacturer->value)))
	cupsFilePrintf(fp, "*%s.Translation NickName/%s %s, %s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(manufacturer->value),
        	       locatalog->find_message(model_name->value),
		       version->value, lf);
      else
	cupsFilePrintf(fp, "*%s.Translation NickName/%s, %s: \"\"%s",
                       locale->value,
        	       locatalog->find_message(model_name->value),
		       version->value, lf);

      // Then the page sizes...
      cupsFilePrintf(fp, "*%s.Translation PageSize/%s: \"\"%s", locale->value,
                     locatalog->find_message("Media Size"), lf);

      for (m = (ppdcMediaSize *)sizes->first();
	   m;
	   m = (ppdcMediaSize *)sizes->next())
      {
        cupsFilePrintf(fp, "*%s.PageSize %s/%s: \"\"%s", locale->value,
        	       m->name->value, locatalog->find_message(m->text->value),
		       lf);
      }

      // Next the groups and options...
      for (g = (ppdcGroup *)groups->first(); g; g = (ppdcGroup *)groups->next())
      {
	if (!g->options->count)
	  continue;

	if (strcasecmp(g->name->value, "General"))
	  cupsFilePrintf(fp, "*%s.Translation %s/%s: \"\"%s", locale->value,
	                 g->name->value,
                	 locatalog->find_message(g->text->value), lf);

	for (o = (ppdcOption *)g->options->first();
             o;
	     o = (ppdcOption *)g->options->next())
	{
	  if (!o->choices->count)
            continue;

          cupsFilePrintf(fp, "*%s.Translation %s/%s: \"\"%s", locale->value,
	                 o->name->value,
			 locatalog->find_message(o->text->value ?
			                         o->text->value :
						 o->name->value), lf);

	  for (c = (ppdcChoice *)o->choices->first();
               c;
	       c = (ppdcChoice *)o->choices->next())
	  {
            // Write this choice...
            cupsFilePrintf(fp, "*%s.%s %s/%s: \"\"%s", locale->value,
	                   o->name->value, c->name->value,
			   locatalog->find_message(c->text->value ?
			                           c->text->value :
						   c->name->value), lf);
	  }
	}
      }

      // Finally the localizable attributes...
      for (a = (ppdcAttr *)attrs->first(); a; a = (ppdcAttr *)attrs->next())
      {
        if ((!a->text || !a->text->value || !a->text->value[0]) &&
	    strncmp(a->name->value, "Custom", 6) &&
	    strncmp(a->name->value, "ParamCustom", 11))
	  continue;

        if (!a->localizable &&
	    strcmp(a->name->value, "APCustomColorMatchingName") &&
	    strcmp(a->name->value, "APPrinterPreset") &&
	    strcmp(a->name->value, "cupsICCProfile") &&
	    strcmp(a->name->value, "cupsIPPReason") &&
	    strcmp(a->name->value, "cupsMarkerName") &&
	    strncmp(a->name->value, "Custom", 6) &&
	    strncmp(a->name->value, "ParamCustom", 11))
          continue;

        cupsFilePrintf(fp, "*%s.%s %s/%s: \"%s\"%s", locale->value,
	               a->name->value, a->selector->value,
		       locatalog->find_message(a->text && a->text->value ?
		                               a->text->value : a->name->value),
		       ((a->localizable && a->value->value[0]) ||
		        !strcmp(a->name->value, "cupsIPPReason")) ?
		           locatalog->find_message(a->value->value) : "",
		       lf);
      }
    }
  }

  if (default_font && default_font->value)
    cupsFilePrintf(fp, "*DefaultFont: %s%s", default_font->value, lf);
  else
    cupsFilePrintf(fp, "*DefaultFont: Courier%s", lf);

  for (fn = (ppdcFont *)fonts->first(); fn; fn = (ppdcFont *)fonts->next())
    if (!strcmp(fn->name->value, "*"))
    {
      for (bfn = (ppdcFont *)src->base_fonts->first();
	   bfn;
	   bfn = (ppdcFont *)src->base_fonts->next())
	cupsFilePrintf(fp, "*Font %s: %s \"%s\" %s %s%s",
		       bfn->name->value, bfn->encoding->value,
		       bfn->version->value, bfn->charset->value,
		       bfn->status == PPDC_FONT_ROM ? "ROM" : "Disk", lf);
    }
    else
      cupsFilePrintf(fp, "*Font %s: %s \"%s\" %s %s%s",
        	     fn->name->value, fn->encoding->value, fn->version->value,
		     fn->charset->value,
		     fn->status == PPDC_FONT_ROM ? "ROM" : "Disk", lf);

  cupsFilePrintf(fp, "*%% End of %s, %05d bytes.%s", pc_file_name->value,
        	 (int)(cupsFileTell(fp) + 25 + strlen(pc_file_name->value)),
		 lf);

  if (delete_cat)
    delete catalog;

  return (0);
}


//
// End of "$Id$".
//
