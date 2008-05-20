//
// "$Id$"
//
//   Source class for the CUPS PPD Compiler.
//
//   Copyright 2007-2008 by Apple Inc.
//   Copyright 2002-2007 by Easy Software Products.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//
// Contents:
//
//   ppdcSource::ppdcSource()         - Load a driver source file.
//   ppdcSource::~ppdcSource()        - Free a driver source file.
//   ppdcSource::add_include()        - Add an include directory.
//   ppdcSource::find_driver()        - Find a driver.
//   ppdcSource::find_include()       - Find an include file.
//   ppdcSource::find_size()          - Find a media size.
//   ppdcSource::find_variable()      - Find a variable.
//   ppdcSource::get_attr()           - Get an attribute.
//   ppdcSource::get_boolean()        - Get a boolean value.
//   ppdcSource::get_choice()         - Get a choice.
//   ppdcSource::get_color_model()    - Get an old-style color model option.
//   ppdcSource::get_color_order()    - Get an old-style color order value.
//   ppdcSource::get_color_profile()  - Get a color profile definition.
//   ppdcSource::get_color_space()    - Get an old-style colorspace value.
//   ppdcSource::get_constraint()     - Get a constraint.
//   ppdcSource::get_custom_size()    - Get a custom media size definition
//                                      from a file.
//   ppdcSource::get_filter()         - Get a filter.
//   ppdcSource::get_float()          - Get a single floating-point number.
//   ppdcSource::get_font()           - Get a font definition.
//   ppdcSource::get_generic()        - Get a generic old-style option.
//   ppdcSource::get_group()          - Get an option group.
//   ppdcSource::get_installable()    - Get an installable option.
//   ppdcSource::get_integer()        - Get an integer value from a string.
//   ppdcSource::get_integer()        - Get an integer value from a file.
//   ppdcSource::get_measurement()    - Get a measurement value.
//   ppdcSource::get_option()         - Get an option definition.
//   ppdcSource::get_resolution()     - Get an old-style resolution option.
//   ppdcSource::get_simple_profile() - Get a simple color profile definition.
//   ppdcSource::get_size()           - Get a media size definition from a file.
//   ppdcSource::get_token()          - Get a token from a file.
//   ppdcSource::get_variable()       - Get a variable definition.
//   ppdcSource::quotef()             - Write a formatted, quoted string...
//   ppdcSource::read_file()          - Read a driver source file.
//   ppdcSource::scan_file()          - Scan a driver source file.
//   ppdcSource::set_variable()       - Set a variable.
//   ppdcSource::write_file()         - Write the current source data to a file.
//

//
// Include necessary headers...
//

#include "ppdc.h"
#include <cups/globals.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <cups/raster.h>
#include "data/epson.h"
#include "data/escp.h"
#include "data/hp.h"
#include "data/label.h"
#include "data/pcl.h"


//
// Class globals...
//

ppdcArray	*ppdcSource::includes = 0;
const char	*ppdcSource::driver_types[] =
		{
		  "custom",
		  "ps",
		  "escp",
		  "pcl",
		  "label",
		  "epson",
		  "hp"
		};


//
// 'ppdcSource::ppdcSource()' - Load a driver source file.
//

ppdcSource::ppdcSource(const char *f)	// I - File to read
{
  filename   = new ppdcString(f);
  base_fonts = new ppdcArray();
  drivers    = new ppdcArray();
  po_files   = new ppdcArray();
  sizes      = new ppdcArray();
  vars       = new ppdcArray();

  if (f)
    read_file(f);
}


//
// 'ppdcSource::~ppdcSource()' - Free a driver source file.
//

ppdcSource::~ppdcSource()
{
  delete filename;
  delete base_fonts;
  delete drivers;
  delete po_files;
  delete sizes;
  delete vars;
}


//
// 'ppdcSource::add_include()' - Add an include directory.
//

void
ppdcSource::add_include(const char *d)	// I - Include directory
{
  if (!d)
    return;

  if (!includes)
    includes = new ppdcArray();

  includes->add(new ppdcString(d));
}


//
// 'ppdcSource::find_driver()' - Find a driver.
//

ppdcDriver *				// O - Driver
ppdcSource::find_driver(const char *f)	// I - Driver file name
{
  ppdcDriver	*d;			// Current driver


  for (d = (ppdcDriver *)drivers->first(); d; d = (ppdcDriver *)drivers->next())
    if (!strcasecmp(f, d->pc_file_name->value))
      return (d);

  return (NULL);
}


//
// 'ppdcSource::find_include()' - Find an include file.
//

char *					// O - Found path or NULL
ppdcSource::find_include(
    const char *f,			// I - Include filename
    const char *base,			// I - Current directory
    char       *n,			// I - Path buffer
    int        nlen)			// I - Path buffer length
{
  ppdcString	*dir;			// Include directory
  char		temp[1024],		// Temporary path
		*ptr;			// Pointer to end of path


  // Range check input...
  if (!f || !*f || !n || nlen < 2)
    return (0);

  // Check the first character to see if we have <name> or "name"...
  if (*f == '<')
  {
    // Remove the surrounding <> from the name...
    strlcpy(temp, f + 1, sizeof(temp));
    ptr = temp + strlen(temp) - 1;

    if (*ptr != '>')
    {
      fprintf(stderr, "ppdc: Invalid #include/#po filename \"%s\"!\n", n);
      return (0);
    }

    *ptr = '\0';
    f    = temp;
  }
  else
  {
    // Check for the local file relative to the current directory...
    if (base && *base && f[0] != '/')
      snprintf(n, nlen, "%s/%s", base, f);
    else
      strlcpy(n, f, nlen);

    if (!access(n, 0))
      return (n);
    else if (*f == '/')
    {
      // Absolute path that doesn't exist...
      return (0);
    }
  }

  // Search the include directories, if any...
  if (includes)
  {
    for (dir = (ppdcString *)includes->first(); dir; dir = (ppdcString *)includes->next())
    {
      snprintf(n, nlen, "%s/%s", dir->value, f);
      if (!access(n, 0))
        return (n);
    }
  }

  // Search the standard include directories...
  _cups_globals_t *cg = _cupsGlobals();	// Global data

  snprintf(n, nlen, "%s/ppdc/%s", cg->cups_datadir, f);
  if (!access(n, 0))
    return (n);

  snprintf(n, nlen, "%s/po/%s", cg->cups_datadir, f);
  if (!access(n, 0))
    return (n);
  else
    return (0);
}


//
// 'ppdcSource::find_po()' - Find a message catalog for the given locale...
//

ppdcCatalog *				// O - Message catalog or NULL
ppdcSource::find_po(const char *l)	// I - Locale name
{
  ppdcCatalog	*cat;			// Current message catalog


  for (cat = (ppdcCatalog *)po_files->first();
       cat;
       cat = (ppdcCatalog *)po_files->next())
    if (!strcasecmp(l, cat->locale->value))
      return (cat);

  return (NULL);
}


//
// 'ppdcSource::find_size()' - Find a media size.
//

ppdcMediaSize *				// O - Size
ppdcSource::find_size(const char *s)	// I - Size name
{
  ppdcMediaSize	*m;			// Current media size


  for (m = (ppdcMediaSize *)sizes->first(); m; m = (ppdcMediaSize *)sizes->next())
    if (!strcasecmp(s, m->name->value))
      return (m);

  return (NULL);
}


//
// 'ppdcSource::find_variable()' - Find a variable.
//

ppdcVariable *				// O - Variable
ppdcSource::find_variable(const char *n)// I - Variable name
{
  ppdcVariable	*v;			// Current variable


  for (v = (ppdcVariable *)vars->first(); v; v = (ppdcVariable *)vars->next())
    if (!strcasecmp(n, v->name->value))
      return (v);

  return (NULL);
}


//
// 'ppdcSource::get_attr()' - Get an attribute.
//

ppdcAttr *				// O - Attribute
ppdcSource::get_attr(ppdcFile *fp)	// I - File to read
{
  char	name[1024],			// Name string
	selector[1024],			// Selector string
	*text,				// Text string
	value[1024];			// Value string


  // Get the attribute parameters:
  //
  // Attribute name selector value
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected name after Attribute on line %d of %s!\n",
            fp->line, fp->filename);
    return (0);
  }

  if (!get_token(fp, selector, sizeof(selector)))
  {
    fprintf(stderr, "ppdc: Expected selector after Attribute on line %d of %s!\n",
            fp->line, fp->filename);
    return (0);
  }

  if ((text = strchr(selector, '/')) != NULL)
    *text++ = '\0';

  if (!get_token(fp, value, sizeof(value)))
  {
    fprintf(stderr, "ppdc: Expected value after Attribute on line %d of %s!\n",
            fp->line, fp->filename);
    return (0);
  }

//  printf("name=\"%s\", selector=\"%s\", value=\"%s\"\n", name, selector, value);

  return (new ppdcAttr(name, selector, text, value));
}


//
// 'ppdcSource::get_boolean()' - Get a boolean value.
//

int					// O - Boolean value
ppdcSource::get_boolean(ppdcFile *fp)	// I - File to read
{
  char	buffer[256];			// String buffer


  if (!get_token(fp, buffer, sizeof(buffer)))
  {
    fprintf(stderr, "ppdc: Expected boolean value on line %d of %s.\n",
            fp->line, fp->filename);
    return (-1);
  }

  if (!strcasecmp(buffer, "on") ||
      !strcasecmp(buffer, "yes") ||
      !strcasecmp(buffer, "true"))
    return (1);
  else if (!strcasecmp(buffer, "off") ||
	   !strcasecmp(buffer, "no") ||
	   !strcasecmp(buffer, "false"))
    return (0);
  else
  {
    fprintf(stderr, "ppdc: Bad boolean value (%s) on line %d of %s.\n",
            buffer, fp->line, fp->filename);
    return (-1);
  }
}


//
// 'ppdcSource::get_choice()' - Get a choice.
//

ppdcChoice *				// O - Choice data
ppdcSource::get_choice(ppdcFile *fp)	// I - File to read
{
  char	name[1024],			// Name
	*text,				// Text
	code[10240];			// Code


  // Read a choice from the file:
  //
  // Choice name/text code
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected choice name/text on line %d of %s.\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if (!get_token(fp, code, sizeof(code)))
  {
    fprintf(stderr, "ppdc: Expected choice code on line %d of %s.\n",
            fp->line, fp->filename);
    return (NULL);
  }

  // Return the new choice
  return (new ppdcChoice(name, text, code));
}


//
// 'ppdcSource::get_color_model()' - Get an old-style color model option.
//

ppdcChoice *				// O - Choice data
ppdcSource::get_color_model(ppdcFile *fp)
					// I - File to read
{
  char		name[1024],		// Option name
		*text,			// Text option
		temp[256];		// Temporary string
  int		color_space,		// Colorspace
		color_order,		// Color order
		compression;		// Compression mode


  // Get the ColorModel parameters:
  //
  // ColorModel name/text colorspace colororder compression
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr,
            "ppdc: Expected name/text combination for ColorModel on line "
	    "%d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr,
            "ppdc: Expected colorspace for ColorModel on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((color_space = get_color_space(temp)) < 0)
    color_space = get_integer(temp);

  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr,
            "ppdc: Expected color order for ColorModel on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((color_order = get_color_order(temp)) < 0)
    color_order = get_integer(temp);

  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr,
            "ppdc: Expected compression for ColorModel on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  compression = get_integer(temp);

  snprintf(temp, sizeof(temp),
           "<</cupsColorSpace %d/cupsColorOrder %d/cupsCompression %d>>"
	   "setpagedevice",
           color_space, color_order, compression);

  return (new ppdcChoice(name, text, temp));
}


//
// 'ppdcSource::get_color_order()' - Get an old-style color order value.
//

int					// O - Color order value
ppdcSource::get_color_order(
    const char *co)			// I - Color order string
{
  if (!strcasecmp(co, "chunked") ||
      !strcasecmp(co, "chunky"))
    return (CUPS_ORDER_CHUNKED);
  else if (!strcasecmp(co, "banded"))
    return (CUPS_ORDER_BANDED);
  else if (!strcasecmp(co, "planar"))
    return (CUPS_ORDER_PLANAR);
  else
    return (-1);
}


//
// 'ppdcSource::get_color_profile()' - Get a color profile definition.
//

ppdcProfile *				// O - Color profile
ppdcSource::get_color_profile(
    ppdcFile *fp)			// I - File to read
{
  char		resolution[1024],	// Resolution/media type
		*media_type;		// Media type
  int		i;			// Looping var
  float		g,			// Gamma value
		d,			// Density value
		m[9];			// Transform matrix


  // Get the ColorProfile parameters:
  //
  // ColorProfile resolution/mediatype gamma density m00 m01 m02 ... m22
  if (!get_token(fp, resolution, sizeof(resolution)))
  {
    fprintf(stderr, "ppdc: Expected resolution/mediatype following ColorProfile on line %d of %s!\n",
	    fp->line, fp->filename);
    return (NULL);
  }

  if ((media_type = strchr(resolution, '/')) != NULL)
    *media_type++ = '\0';
  else
    media_type = resolution;

  g = get_float(fp);
  d = get_float(fp);
  for (i = 0; i < 9; i ++)
    m[i] = get_float(fp);

  return (new ppdcProfile(resolution, media_type, g, d, m));
}


//
// 'ppdcSource::get_color_space()' - Get an old-style colorspace value.
//

int					// O - Colorspace value
ppdcSource::get_color_space(
    const char *cs)			// I - Colorspace string
{
  if (!strcasecmp(cs, "w"))
    return (CUPS_CSPACE_W);
  else if (!strcasecmp(cs, "rgb"))
    return (CUPS_CSPACE_RGB);
  else if (!strcasecmp(cs, "rgba"))
    return (CUPS_CSPACE_RGBA);
  else if (!strcasecmp(cs, "k"))
    return (CUPS_CSPACE_K);
  else if (!strcasecmp(cs, "cmy"))
    return (CUPS_CSPACE_CMY);
  else if (!strcasecmp(cs, "ymc"))
    return (CUPS_CSPACE_YMC);
  else if (!strcasecmp(cs, "cmyk"))
    return (CUPS_CSPACE_CMYK);
  else if (!strcasecmp(cs, "ymck"))
    return (CUPS_CSPACE_YMCK);
  else if (!strcasecmp(cs, "kcmy"))
    return (CUPS_CSPACE_KCMY);
  else if (!strcasecmp(cs, "kcmycm"))
    return (CUPS_CSPACE_KCMYcm);
  else if (!strcasecmp(cs, "gmck"))
    return (CUPS_CSPACE_GMCK);
  else if (!strcasecmp(cs, "gmcs"))
    return (CUPS_CSPACE_GMCS);
  else if (!strcasecmp(cs, "white"))
    return (CUPS_CSPACE_WHITE);
  else if (!strcasecmp(cs, "gold"))
    return (CUPS_CSPACE_GOLD);
  else if (!strcasecmp(cs, "silver"))
    return (CUPS_CSPACE_SILVER);
  else if (!strcasecmp(cs, "CIEXYZ"))
    return (CUPS_CSPACE_CIEXYZ);
  else if (!strcasecmp(cs, "CIELab"))
    return (CUPS_CSPACE_CIELab);
  else if (!strcasecmp(cs, "RGBW"))
    return (CUPS_CSPACE_RGBW);
  else if (!strcasecmp(cs, "ICC1"))
    return (CUPS_CSPACE_ICC1);
  else if (!strcasecmp(cs, "ICC2"))
    return (CUPS_CSPACE_ICC2);
  else if (!strcasecmp(cs, "ICC3"))
    return (CUPS_CSPACE_ICC3);
  else if (!strcasecmp(cs, "ICC4"))
    return (CUPS_CSPACE_ICC4);
  else if (!strcasecmp(cs, "ICC5"))
    return (CUPS_CSPACE_ICC5);
  else if (!strcasecmp(cs, "ICC6"))
    return (CUPS_CSPACE_ICC6);
  else if (!strcasecmp(cs, "ICC7"))
    return (CUPS_CSPACE_ICC7);
  else if (!strcasecmp(cs, "ICC8"))
    return (CUPS_CSPACE_ICC8);
  else if (!strcasecmp(cs, "ICC9"))
    return (CUPS_CSPACE_ICC9);
  else if (!strcasecmp(cs, "ICCA"))
    return (CUPS_CSPACE_ICCA);
  else if (!strcasecmp(cs, "ICCB"))
    return (CUPS_CSPACE_ICCB);
  else if (!strcasecmp(cs, "ICCC"))
    return (CUPS_CSPACE_ICCC);
  else if (!strcasecmp(cs, "ICCD"))
    return (CUPS_CSPACE_ICCD);
  else if (!strcasecmp(cs, "ICCE"))
    return (CUPS_CSPACE_ICCE);
  else if (!strcasecmp(cs, "ICCF"))
    return (CUPS_CSPACE_ICCF);
  else
    return (-1);
}


//
// 'ppdcSource::get_constraint()' - Get a constraint.
//

ppdcConstraint *			// O - Constraint
ppdcSource::get_constraint(ppdcFile *fp)// I - File to read
{
  char		temp[1024],		// One string to rule them all
		*ptr,			// Pointer into string
		*option1,		// Constraint option 1
		*choice1,		// Constraint choice 1
		*option2,		// Constraint option 2
		*choice2;		// Constraint choice 2


  // Read the UIConstaints parameter in one of the following forms:
  //
  // UIConstraints "*Option1 *Option2"
  // UIConstraints "*Option1 Choice1 *Option2"
  // UIConstraints "*Option1 *Option2 Choice2"
  // UIConstraints "*Option1 Choice1 *Option2 Choice2"
  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr, "ppdc: Expected constraints string for UIConstraints on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  for (ptr = temp; isspace(*ptr); ptr ++);

  if (*ptr != '*')
  {
    fprintf(stderr, "ppdc: Option constraint must *name on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  option1 = ptr;

  for (; *ptr && !isspace(*ptr); ptr ++);
  for (; isspace(*ptr); *ptr++ = '\0');

  if (*ptr != '*')
  {
    choice1 = ptr;

    for (; *ptr && !isspace(*ptr); ptr ++);
    for (; isspace(*ptr); *ptr++ = '\0');
  }
  else
    choice1 = NULL;

  if (*ptr != '*')
  {
    fprintf(stderr, "ppdc: Expected two option names on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  option2 = ptr;

  for (; *ptr && !isspace(*ptr); ptr ++);
  for (; isspace(*ptr); *ptr++ = '\0');

  if (*ptr)
    choice2 = ptr;
  else
    choice2 = NULL;

  return (new ppdcConstraint(option1, choice1, option2, choice2));
}


//
// 'ppdcSource::get_custom_size()' - Get a custom media size definition from a file.
//

ppdcMediaSize *				// O - Media size
ppdcSource::get_custom_size(ppdcFile *fp)
					// I - File to read
{
  char		name[1024],		// Name
		*text,			// Text
		size_code[10240],	// PageSize code
		region_code[10240];	// PageRegion
  float		width,			// Width
		length,			// Length
		left,			// Left margin
		bottom,			// Bottom margin
		right,			// Right margin
		top;			// Top margin


  // Get the name, text, width, length, margins, and code:
  //
  // CustomMedia name/text width length left bottom right top size-code region-code
  if (!get_token(fp, name, sizeof(name)))
    return (NULL);

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if ((width = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((length = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((left = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((bottom = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((right = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((top = get_measurement(fp)) < 0.0f)
    return (NULL);

  if (!get_token(fp, size_code, sizeof(size_code)))
    return (NULL);

  if (!get_token(fp, region_code, sizeof(region_code)))
    return (NULL);

  // Return the new media size...
  return (new ppdcMediaSize(name, text, width, length, left, bottom,
                            right, top, size_code, region_code));
}


//
// 'ppdcSource::get_duplex()' - Get a duplex option.
//

void
ppdcSource::get_duplex(ppdcFile   *fp,	// I - File to read from
                       ppdcDriver *d)	// I - Current driver
{
  char		temp[256];		// Duplex keyword
  ppdcAttr	*attr;			// cupsFlipDuplex attribute
  ppdcGroup	*g;			// Current group
  ppdcOption	*o;			// Duplex option


  // Duplex {boolean|none|normal|flip}
  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr, "ppdc: Expected duplex type after Duplex on line %d of %s!\n",
	    fp->line, fp->filename);
    return;
  }

  if (!strcasecmp(temp, "none") || !strcasecmp(temp, "false") ||
      !strcasecmp(temp, "no") || !strcasecmp(temp, "off"))
  {
    g = d->find_group("General");
    if ((o = g->find_option("Duplex")) != NULL)
      g->options->remove(o);

    for (attr = (ppdcAttr *)d->attrs->first();
         attr;
	 attr = (ppdcAttr *)d->attrs->next())
      if (!strcmp(attr->name->value, "cupsFlipDuplex"))
      {
        d->attrs->remove(attr);
	break;
      }
  }
  else if (!strcasecmp(temp, "normal") || !strcasecmp(temp, "true") ||
	   !strcasecmp(temp, "yes") || !strcasecmp(temp, "on") ||
	   !strcasecmp(temp, "flip"))
  {
    g = d->find_group("General");
    o = g->find_option("Duplex");

    if (!o)
    {
      o = new ppdcOption(PPDC_PICKONE, "Duplex", "2-Sided Printing",
                	 !strcasecmp(temp, "flip") ? PPDC_SECTION_PAGE :
			                             PPDC_SECTION_ANY, 10.0f);
      o->add_choice(new ppdcChoice("None", "Off (1-Sided)",
                        	   "<</Duplex false>>setpagedevice"));
      o->add_choice(new ppdcChoice("DuplexNoTumble", "Long-Edge (Portrait)",
                                   "<</Duplex true/Tumble false>>setpagedevice"));
      o->add_choice(new ppdcChoice("DuplexTumble", "Short-Edge (Landscape)",
                                   "<</Duplex true/Tumble true>>setpagedevice"));

      g->add_option(o);
    }

    for (attr = (ppdcAttr *)d->attrs->first();
         attr;
	 attr = (ppdcAttr *)d->attrs->next())
      if (!strcmp(attr->name->value, "cupsFlipDuplex"))
      {
        if (strcasecmp(temp, "flip"))
          d->attrs->remove(attr);
	break;
      }

    if (!strcasecmp(temp, "flip") && !attr)
      d->add_attr(new ppdcAttr("cupsFlipDuplex", NULL, NULL, "true"));
  }
  else
    fprintf(stderr, "ppdc: Unknown duplex type \"%s\" on line %d of %s!\n",
	    temp, fp->line, fp->filename);
}


//
// 'ppdcSource::get_filter()' - Get a filter.
//

ppdcFilter *				// O - Filter
ppdcSource::get_filter(ppdcFile *fp)	// I - File to read
{
  char	type[1024],			// MIME type
	program[1024],			// Filter program
	*ptr;				// Pointer into MIME type
  int	cost;				// Relative cost


  // Read filter parameters in one of the following formats:
  //
  // Filter "type cost program"
  // Filter type cost program

  if (!get_token(fp, type, sizeof(type)))
  {
    fprintf(stderr, "ppdc: Expected a filter definition on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((ptr = strchr(type, ' ')) != NULL)
  {
    // Old-style filter definition in one string...
    *ptr++ = '\0';
    cost = strtol(ptr, &ptr, 10);

    while (isspace(*ptr))
      ptr ++;

    strcpy(program, ptr);
  }
  else
  {
    cost = get_integer(fp);

    if (!get_token(fp, program, sizeof(program)))
    {
      fprintf(stderr, "ppdc: Expected a program name on line %d of %s!\n",
              fp->line, fp->filename);
      return (NULL);
    }
  }

  if (!type[0])
  {
    fprintf(stderr, "ppdc: Invalid empty MIME type for filter on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if (cost < 0 || cost > 200)
  {
    fprintf(stderr, "ppdc: Invalid cost for filter on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if (!program[0])
  {
    fprintf(stderr, "ppdc: Invalid empty program name for filter on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  return (new ppdcFilter(type, program, cost));
}


//
// 'ppdcSource::get_float()' - Get a single floating-point number.
//

float					// O - Number
ppdcSource::get_float(ppdcFile *fp)	// I - File to read
{
  char	temp[256],			// String buffer
	*ptr;				// Pointer into buffer
  float	val;				// Floating point value


  // Get the number from the file and range-check...
  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr, "ppdc: Expected real number on line %d of %s!\n",
            fp->line, fp->filename);
    return (-1.0f);
  }

  val = (float)strtod(temp, &ptr);

  if (*ptr)
  {
    fprintf(stderr, "ppdc: Unknown trailing characters in real number \"%s\" on line %d of %s!\n",
            temp, fp->line, fp->filename);
    return (-1.0f);
  }
  else
    return (val);
}


//
// 'ppdcSource::get_font()' - Get a font definition.
//

ppdcFont *				// O - Font data
ppdcSource::get_font(ppdcFile *fp)	// I - File to read
{
  char			name[256],	// Font name
			encoding[256],	// Font encoding
			version[256],	// Font version
			charset[256],	// Font charset
			temp[256];	// Font status string
  ppdcFontStatus	status;		// Font status enumeration


  // Read font parameters as follows:
  //
  // Font *
  // Font name encoding version charset status
  // %font name encoding version charset status
  //
  // "Name" is the PostScript font name.
  //
  // "Encoding" is the default encoding of the font: Standard, ISOLatin1,
  // Special, Expert, ExpertSubset, etc.
  //
  // "Version" is the version number string.
  //
  // "Charset" specifies the characters that are included in the font:
  // Standard, Special, Expert, Adobe-Identity, etc.
  //
  // "Status" is the keyword ROM or Disk.
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected name after Font on line %d of %s!\n",
            fp->line, fp->filename);
    return (0);
  }

  if (!strcmp(name, "*"))
  {
    // Include all base fonts...
    encoding[0] = '\0';
    version[0]  = '\0';
    charset[0]  = '\0';
    status      = PPDC_FONT_ROM;
  }
  else
  {
    // Load a full font definition...
    if (!get_token(fp, encoding, sizeof(encoding)))
    {
      fprintf(stderr, "ppdc: Expected encoding after Font on line %d of %s!\n",
              fp->line, fp->filename);
      return (0);
    }

    if (!get_token(fp, version, sizeof(version)))
    {
      fprintf(stderr, "ppdc: Expected version after Font on line %d of %s!\n",
              fp->line, fp->filename);
      return (0);
    }

    if (!get_token(fp, charset, sizeof(charset)))
    {
      fprintf(stderr, "ppdc: Expected charset after Font on line %d of %s!\n",
              fp->line, fp->filename);
      return (0);
    }

    if (!get_token(fp, temp, sizeof(temp)))
    {
      fprintf(stderr, "ppdc: Expected status after Font on line %d of %s!\n",
              fp->line, fp->filename);
      return (0);
    }

    if (!strcasecmp(temp, "ROM"))
      status = PPDC_FONT_ROM;
    else if (!strcasecmp(temp, "Disk"))
      status = PPDC_FONT_DISK;
    else
    {
      fprintf(stderr, "ppdc: Bad status keyword %s on line %d of %s!\n",
              temp, fp->line, fp->filename);
      return (0);
    }
  }

//  printf("Font %s %s %s %s %s\n", name, encoding, version, charset, temp);

  return (new ppdcFont(name, encoding, version, charset, status));
}


//
// 'ppdcSource::get_generic()' - Get a generic old-style option.
//

ppdcChoice *				// O - Choice data
ppdcSource::get_generic(ppdcFile   *fp,	// I - File to read
                        const char *keyword,
					// I - Keyword name
                        const char *tattr,
					// I - Text attribute
			const char *nattr)
					// I - Numeric attribute
{
  char		name[1024],		// Name
		*text,			// Text
		command[256];		// Command string
  int		val;			// Numeric value


  // Read one of the following parameters:
  //
  // Foo name/text
  // Foo integer name/text
  if (nattr)
    val = get_integer(fp);
  else
    val = 0;

  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected name/text after %s on line %d of %s!\n",
            keyword, fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if (nattr)
  {
    if (tattr)
      snprintf(command, sizeof(command),
               "<</%s(%s)/%s %d>>setpagedevice",
               tattr, name, nattr, val);
    else
      snprintf(command, sizeof(command),
               "<</%s %d>>setpagedevice",
               nattr, val);
  }
  else
    snprintf(command, sizeof(command),
             "<</%s(%s)>>setpagedevice",
             tattr, name);

  return (new ppdcChoice(name, text, command));
}


//
// 'ppdcSource::get_group()' - Get an option group.
//

ppdcGroup *				// O - Group
ppdcSource::get_group(ppdcFile   *fp,	// I - File to read
                      ppdcDriver *d)	// I - Printer driver
{
  char		name[1024],		// UI name
		*text;			// UI text
  ppdcGroup	*g;			// Group


  // Read the Group parameters:
  //
  // Group name/text
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected group name/text on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  // See if the group already exists...
  if ((g = d->find_group(name)) == NULL)
  {
    // Nope, add a new one...
    g = new ppdcGroup(name, text);
    d->add_group(g);
  }

  return (g);
}


//
// 'ppdcSource::get_installable()' - Get an installable option.
//

ppdcOption *				// O - Option
ppdcSource::get_installable(ppdcFile *fp)
					// I - File to read
{
  char		name[1024],		// Name for installable option
		*text;			// Text for installable option
  ppdcOption	*o;			// Option


  // Read the parameter for an installable option:
  //
  // Installable name/text
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected name/text after Installable on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  // Create the option...
  o = new ppdcOption(PPDC_BOOLEAN, name, text, PPDC_SECTION_ANY, 10.0f);

  // Add the false and true choices...
  o->add_choice(new ppdcChoice("False", "Not Installed", ""));
  o->add_choice(new ppdcChoice("True", "Installed", ""));

  return (o);
}


//
// 'ppdcSource::get_integer()' - Get an integer value from a string.
//

int					// O - Integer value
ppdcSource::get_integer(const char *v)	// I - Value string
{
  long	val;				// Value
  long	temp;				// Temporary value
  char	*newv;				// New value string pointer


  // Parse the value string...
  if (!v)
    return (-1);

  if (isdigit(*v) || *v == '-' || *v == '+')
  {
    // Return a simple integer value
    val = strtol(v, (char **)&v, 0);
    if (*v || val == LONG_MIN)
      return (-1);
    else
      return ((int)val);
  }
  else if (*v == '(')
  {
    // Return the bitwise OR of each integer in parenthesis...
    v ++;
    val = 0;

    while (*v && *v != ')')
    {
      temp = strtol(v, &newv, 0);

      if (!*newv || newv == v || !(isspace(*newv) || *newv == ')') ||
          temp == LONG_MIN)
        return (-1);

      val |= temp;
      v   = newv;
    }

    if (*v == ')')
      return ((int)val);
    else
      return (-1);
  }
  else
    return (-1);
}


//
// 'ppdcSource::get_integer()' - Get an integer value from a file.
//

int					// O - Integer value
ppdcSource::get_integer(ppdcFile *fp)	// I - File to read
{
  char	temp[1024];			// String buffer


  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr, "ppdc: Expected integer on line %d of %s!\n",
            fp->line, fp->filename);
    return (-1);
  }
  else
    return (get_integer(temp));
}


//
// 'ppdcSource::get_measurement()' - Get a measurement value.
//

float					// O - Measurement value in points
ppdcSource::get_measurement(ppdcFile *fp)
					// I - File to read
{
  char	buffer[256],			// Number buffer
	*ptr;				// Pointer into buffer
  float	val;				// Measurement value


  // Grab a token from the file...
  if (!get_token(fp, buffer, sizeof(buffer)))
    return (-1.0f);

  // Get the floating point value of "s" and skip all digits and decimal points.
  val = (float)strtod(buffer, &ptr);

  // Check for a trailing unit specifier...
  if (!strcasecmp(ptr, "mm"))
    val *= 72.0f / 25.4f;
  else if (!strcasecmp(ptr, "cm"))
    val *= 72.0f / 2.54f;
  else if (!strcasecmp(ptr, "m"))
    val *= 72.0f / 0.0254f;
  else if (!strcasecmp(ptr, "in"))
    val *= 72.0f;
  else if (!strcasecmp(ptr, "ft"))
    val *= 72.0f * 12.0f;
  else if (strcasecmp(ptr, "pt") && *ptr)
    return (-1.0f);

  return (val);
}


//
// 'ppdcSource::get_option()' - Get an option definition.
//

ppdcOption *				// O - Option
ppdcSource::get_option(ppdcFile   *fp,	// I - File to read
                       ppdcDriver *d,	// I - Printer driver
		       ppdcGroup  *g)	// I - Current group
{
  char		name[1024],		// UI name
		*text,			// UI text
		type[256];		// UI type string
  ppdcOptType	ot;			// Option type value
  ppdcOptSection section;		// Option section
  float		order;			// Option order
  ppdcOption	*o;			// Option


  // Read the Option parameters:
  //
  // Option name/text type section order
  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected option name/text on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if (!get_token(fp, type, sizeof(type)))
  {
    fprintf(stderr, "ppdc: Expected option type on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if (!strcasecmp(type, "boolean"))
    ot = PPDC_BOOLEAN;
  else if (!strcasecmp(type, "pickone"))
    ot = PPDC_PICKONE;
  else if (!strcasecmp(type, "pickmany"))
    ot = PPDC_PICKMANY;
  else
  {
    fprintf(stderr, "ppdc: Invalid option type \"%s\" on line %d of %s!\n",
            type, fp->line, fp->filename);
    return (NULL);
  }

  if (!get_token(fp, type, sizeof(type)))
  {
    fprintf(stderr, "ppdc: Expected option section on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if (!strcasecmp(type, "AnySetup"))
    section = PPDC_SECTION_ANY;
  else if (!strcasecmp(type, "DocumentSetup"))
    section = PPDC_SECTION_DOCUMENT;
  else if (!strcasecmp(type, "ExitServer"))
    section = PPDC_SECTION_EXIT;
  else if (!strcasecmp(type, "JCLSetup"))
    section = PPDC_SECTION_JCL;
  else if (!strcasecmp(type, "PageSetup"))
    section = PPDC_SECTION_PAGE;
  else if (!strcasecmp(type, "Prolog"))
    section = PPDC_SECTION_PROLOG;
  else
  {
    fprintf(stderr, "ppdc: Invalid option section \"%s\" on line %d of %s!\n",
            type, fp->line, fp->filename);
    return (NULL);
  }

  order = get_float(fp);

  // See if the option already exists...
  if ((o = d->find_option(name)) == NULL)
  {
    // Nope, add a new one...
    o = new ppdcOption(ot, name, text, section, order);
    g->add_option(o);
  }
  else if (o->type != ot)
  {
//    printf("o=%p, o->type=%d, o->name=%s, ot=%d, name=%s\n",
//           o, o->type, o->name->value, ot, name);
    fprintf(stderr, "ppdc: Option already defined with a different type on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  return (o);
}


//
// 'ppdcSource::get_po()' - Get a message catalog.
//

ppdcCatalog *				// O - Message catalog
ppdcSource::get_po(ppdcFile *fp)	// I - File to read
{
  char		locale[32],		// Locale name
		poname[1024],		// Message catalog filename
		basedir[1024],		// Base directory
		*baseptr,		// Pointer into directory
		pofilename[1024];	// Full filename of message catalog
  ppdcCatalog	*cat;			// Message catalog


  // Read the #po parameters:
  //
  // #po locale "filename.po"
  if (!get_token(fp, locale, sizeof(locale)))
  {
    fprintf(stderr, "ppdc: Expected locale after #po on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if (!get_token(fp, poname, sizeof(poname)))
  {
    fprintf(stderr, "ppdc: Expected filename after #po %s on line %d of %s!\n",
            locale, fp->line, fp->filename);
    return (NULL);
  }

  // See if the locale is already loaded...
  if (find_po(locale))
  {
    fprintf(stderr, "ppdc: Duplicate #po for locale %s on line %d of %s!\n",
            locale, fp->line, fp->filename);
    return (NULL);
  }

  // Figure out the current directory...
  strlcpy(basedir, fp->filename, sizeof(basedir));

  if ((baseptr = strrchr(basedir, '/')) != NULL)
    *baseptr = '\0';
  else
    strcpy(basedir, ".");

  // Find the po file...
  if (find_include(poname, basedir, pofilename, sizeof(pofilename)))
  {
    // Found it, so load it...
    cat = new ppdcCatalog(locale, pofilename);

    // Reset the filename to the name supplied by the user...
    delete cat->filename;
    cat->filename = new ppdcString(poname);

    // Return the catalog...
    return (cat);
  }
  else
  {
    fprintf(stderr, "ppdc: Unable to find #po file %s on line %d of %s!\n",
            poname, fp->line, fp->filename);
    return (NULL);
  }
}


//
// 'ppdcSource::get_resolution()' - Get an old-style resolution option.
//

ppdcChoice *				// O - Choice data
ppdcSource::get_resolution(ppdcFile *fp)// I - File to read
{
  char		name[1024],		// Name
		*text,			// Text
		temp[256],		// Temporary string
		command[256],		// Command string
		*commptr;		// Pointer into command
  int		xdpi, ydpi,		// X + Y resolution
		color_order,		// Color order
		color_space,		// Colorspace
		compression,		// Compression mode
		depth,			// Bits per color
		row_count,		// Row count
		row_feed,		// Row feed
		row_step;		// Row step/interval


  // Read the resolution parameters:
  //
  // Resolution colorspace bits row-count row-feed row-step name/text
  if (!get_token(fp, temp, sizeof(temp)))
  {
    fprintf(stderr, "ppdc: Expected override field after Resolution on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  color_order = get_color_order(temp);
  color_space = get_color_space(temp);
  compression = get_integer(temp);

  depth       = get_integer(fp);
  row_count   = get_integer(fp);
  row_feed    = get_integer(fp);
  row_step    = get_integer(fp);

  if (!get_token(fp, name, sizeof(name)))
  {
    fprintf(stderr, "ppdc: Expected name/text after Resolution on line %d of %s!\n",
            fp->line, fp->filename);
    return (NULL);
  }

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  switch (sscanf(name, "%dx%d", &xdpi, &ydpi))
  {
    case 0 :
        fprintf(stderr, "ppdc: Bad resolution name \"%s\" on line %d of %s!\n",
	        name, fp->line, fp->filename);
        break;
    case 1 :
        ydpi = xdpi;
	break;
  }

  // Create the necessary PS commands...
  snprintf(command, sizeof(command),
           "<</HWResolution[%d %d]/cupsBitsPerColor %d/cupsRowCount %d"
           "/cupsRowFeed %d/cupsRowStep %d",
	   xdpi, ydpi, depth, row_count, row_feed, row_step);
  commptr = command + strlen(command);

  if (color_order >= 0)
  {
    snprintf(commptr, sizeof(command) - (commptr - command),
             "/cupsColorOrder %d", color_order);
    commptr += strlen(commptr);
  }

  if (color_space >= 0)
  {
    snprintf(commptr, sizeof(command) - (commptr - command),
             "/cupsColorSpace %d", color_space);
    commptr += strlen(commptr);
  }

  if (compression >= 0)
  {
    snprintf(commptr, sizeof(command) - (commptr - command),
             "/cupsCompression %d", compression);
    commptr += strlen(commptr);
  }

  snprintf(commptr, sizeof(command) - (commptr - command), ">>setpagedevice");

  // Return the new choice...
  return (new ppdcChoice(name, text, command));
}


//
// 'ppdcSource::get_simple_profile()' - Get a simple color profile definition.
//

ppdcProfile *				// O - Color profile
ppdcSource::get_simple_profile(ppdcFile *fp)
					// I - File to read
{
  char		resolution[1024],	// Resolution/media type
		*media_type;		// Media type
  float		m[9];			// Transform matrix
  float		kd, rd, g;		// Densities and gamma
  float		red, green, blue;	// RGB adjustments
  float		yellow;			// Yellow density
  float		color;			// Color density values


  // Get the SimpleColorProfile parameters:
  //
  // SimpleColorProfile resolution/mediatype black-density yellow-density
  //     red-density gamma red-adjust green-adjust blue-adjust
  if (!get_token(fp, resolution, sizeof(resolution)))
  {
    fprintf(stderr, "ppdc: Expected resolution/mediatype following SimpleColorProfile on line %d of %s!\n",
	    fp->line, fp->filename);
    return (NULL);
  }

  if ((media_type = strchr(resolution, '/')) != NULL)
    *media_type++ = '\0';
  else
    media_type = resolution;

  // Collect the profile parameters...
  kd     = get_float(fp);
  yellow = get_float(fp);
  rd     = get_float(fp);
  g      = get_float(fp);
  red    = get_float(fp);
  green  = get_float(fp);
  blue   = get_float(fp);

  // Build the color profile...
  color = 0.5f * rd / kd - kd;
  m[0]  = 1.0f;				// C
  m[1]  = color + blue;			// C + M (blue)
  m[2]  = color - green;		// C + Y (green)
  m[3]  = color - blue;			// M + C (blue)
  m[4]  = 1.0f;				// M
  m[5]  = color + red;			// M + Y (red)
  m[6]  = yellow * (color + green);	// Y + C (green)
  m[7]  = yellow * (color - red);	// Y + M (red)
  m[8]  = yellow;			// Y

  if (m[1] > 0.0f)
  {
    m[3] -= m[1];
    m[1] = 0.0f;
  }
  else if (m[3] > 0.0f)
  {
    m[1] -= m[3];
    m[3] = 0.0f;
  }

  if (m[2] > 0.0f)
  {
    m[6] -= m[2];
    m[2] = 0.0f;
  }
  else if (m[6] > 0.0f)
  {
    m[2] -= m[6];
    m[6] = 0.0f;
  }

  if (m[5] > 0.0f)
  {
    m[7] -= m[5];
    m[5] = 0.0f;
  }
  else if (m[7] > 0.0f)
  {
    m[5] -= m[7];
    m[7] = 0.0f;
  }

  // Return the new profile...
  return (new ppdcProfile(resolution, media_type, g, kd, m));
}


//
// 'ppdcSource::get_size()' - Get a media size definition from a file.
//

ppdcMediaSize *				// O - Media size
ppdcSource::get_size(ppdcFile *fp)	// I - File to read
{
  char		name[1024],		// Name
		*text;			// Text
  float		width,			// Width
		length;			// Length


  // Get the name, text, width, and length:
  //
  // #media name/text width length
  if (!get_token(fp, name, sizeof(name)))
    return (NULL);

  if ((text = strchr(name, '/')) != NULL)
    *text++ = '\0';
  else
    text = name;

  if ((width = get_measurement(fp)) < 0.0f)
    return (NULL);

  if ((length = get_measurement(fp)) < 0.0f)
    return (NULL);

  // Return the new media size...
  return (new ppdcMediaSize(name, text, width, length, 0.0f, 0.0f, 0.0f, 0.0f));
}


//
// 'ppdcSource::get_token()' - Get a token from a file.
//

char *					// O - Token string or NULL
ppdcSource::get_token(ppdcFile *fp,	// I - File to read
                      char     *buffer,	// I - Buffer
		      int      buflen)	// I - Length of buffer
{
  char		*bufptr,		// Pointer into string buffer
		*bufend;		// End of string buffer
  int		ch,			// Character from file
		nextch,			// Next char in file
		quote,			// Quote character used...
		empty,			// Empty input?
		startline;		// Start line for quote
  char		name[256],		// Name string
		*nameptr;		// Name pointer
  ppdcVariable	*var;			// Variable pointer


  // Mark the beginning and end of the buffer...
  bufptr = buffer;
  bufend = buffer + buflen - 1;

  // Loop intil we've read a token...
  quote     = 0;
  startline = 0;
  empty     = 1;

  while ((ch = fp->get()) != EOF)
  {
    if (isspace(ch) && !quote)
    {
      if (empty)
        continue;
      else
        break;
    }
    else if (ch == '$')
    {
      // Variable substitution
      empty = 0;

      for (nameptr = name; (ch = fp->peek()) != EOF;)
      {
        if (!isalnum(ch) && ch != '_')
	  break;
	else if (nameptr < (name + sizeof(name) - 1))
	  *nameptr++ = fp->get();
      }

      if (nameptr == name)
      {
        // Just substitute this character...
	if (ch == '$')
	{
	  // $$ = $
	  if (bufptr < bufend)
	    *bufptr++ = fp->get();
	}
	else
	{
	  // $ch = $ch
          fprintf(stderr, "ppdc: Bad variable substitution ($%c) on line %d of %s.\n",
                  ch, fp->line, fp->filename);

	  if (bufptr < bufend)
	    *bufptr++ = '$';
	}
      }
      else
      {
        // Substitute the variable value...
	*nameptr = '\0';
	var = find_variable(name);
	if (var)
	{
	  strncpy(bufptr, var->value->value, bufend - bufptr);
	  bufptr += strlen(var->value->value);
	}
	else
	{
	  fprintf(stderr, "ppdc: Undefined variable (%s) on line %d of %s.\n",
        	  name, fp->line, fp->filename);
	  snprintf(bufptr, bufend - bufptr + 1, "$%s", name);
	  bufptr += strlen(name) + 1;
	}
      }
    }
    else if (ch == '/' && !quote)
    {
      // Possibly a comment...
      nextch = fp->peek();

      if (nextch == '*')
      {
        // C comment...
	fp->get();
	ch = fp->get();
	while ((nextch = fp->get()) != EOF)
	{
	  if (ch == '*' && nextch == '/')
	    break;

	  ch = nextch;
	}

        if (nextch == EOF)
          break;
      }
      else if (nextch == '/')
      {
        // C++ comment...
        while ((nextch = fp->get()) != EOF)
          if (nextch == '\n')
	    break;

        if (nextch == EOF)
          break;
      }
      else
      {
        // Not a comment...
        empty = 0;

	if (bufptr < bufend)
	  *bufptr++ = ch;
      }
    }
    else if (ch == '\'' || ch == '\"')
    {
      empty = 0;

      if (quote == ch)
      {
        // Ending the current quoted string...
        quote = 0;
      }
      else if (quote)
      {
        // Insert the opposing quote char...
	if (bufptr < bufend)
          *bufptr++ = ch;
      }
      else
      {
        // Start a new quoted string...
        startline = fp->line;
        quote     = ch;
      }
    }
    else if ((ch == '(' || ch == '<') && !quote)
    {
      empty     = 0;
      quote     = ch;
      startline = fp->line;

      if (bufptr < bufend)
	*bufptr++ = ch;
    }
    else if ((ch == ')' && quote == '(') || (ch == '>' && quote == '<'))
    {
      quote = 0;

      if (bufptr < bufend)
	*bufptr++ = ch;
    }
    else if (ch == '\\')
    {
      empty = 0;

      if ((ch = fp->get()) == EOF)
        break;

      if (bufptr < bufend)
        *bufptr++ = ch;
    }
    else if (bufptr < bufend)
    {
      empty = 0;

      *bufptr++ = ch;

      if ((ch == '{' || ch == '}') && !quote)
        break;
    }
  }

  if (quote)
  {
    fprintf(stderr, "ppdc: Unterminated string starting with %c on line %d of %s!\n",
            quote, startline, fp->filename);
    return (NULL);
  }

  if (empty)
    return (NULL);
  else
  {
    *bufptr = '\0';
//    puts(buffer);
    return (buffer);
  }
}


//
// 'ppdcSource::get_variable()' - Get a variable definition.
//

ppdcVariable *				// O - Variable
ppdcSource::get_variable(ppdcFile *fp)	// I - File to read
{
  char		name[1024],		// Name
		value[1024];		// Value


  // Get the name and value:
  //
  // #define name value
  if (!get_token(fp, name, sizeof(name)))
    return (NULL);

  if (!get_token(fp, value, sizeof(value)))
    return (NULL);

  // Set the variable...
  return (set_variable(name, value));
}


//
// 'ppdcSource::quotef()' - Write a formatted, quoted string...
//

int					// O - Number bytes on success, -1 on failure
ppdcSource::quotef(cups_file_t *fp,	// I - File to write to
                   const char  *format,	// I - Printf-style format string
		   ...)			// I - Additional args as needed
{
  va_list	ap;			// Pointer to additional arguments
  int		bytes;			// Bytes written
  char		sign,			// Sign of format width
		size,			// Size character (h, l, L)
		type;			// Format type character
  const char	*bufformat;		// Start of format
  int		width,			// Width of field
		prec;			// Number of characters of precision
  char		tformat[100];		// Temporary format string for fprintf()
  char		*s;			// Pointer to string
  int		slen;			// Length of string
  int		i;			// Looping var


  // Range check input...
  if (!fp || !format)
    return (-1);

  // Loop through the format string, formatting as needed...
  va_start(ap, format);

  bytes = 0;

  while (*format)
  {
    if (*format == '%')
    {
      bufformat = format;
      format ++;

      if (*format == '%')
      {
        cupsFilePutChar(fp, *format++);
	bytes ++;
	continue;
      }
      else if (strchr(" -+#\'", *format))
        sign = *format++;
      else
        sign = 0;

      width = 0;
      while (isdigit(*format))
        width = width * 10 + *format++ - '0';

      if (*format == '.')
      {
        format ++;
	prec = 0;

	while (isdigit(*format))
          prec = prec * 10 + *format++ - '0';
      }
      else
        prec = -1;

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';
	format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
        size = *format++;

      if (!*format)
        break;

      type = *format++;

      switch (type)
      {
	case 'E' : // Floating point formats
	case 'G' :
	case 'e' :
	case 'f' :
	case 'g' :
	    if ((format - bufformat + 1) > (int)sizeof(tformat))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    bytes += cupsFilePrintf(fp, tformat, va_arg(ap, double));
	    break;

        case 'B' : // Integer formats
	case 'X' :
	case 'b' :
        case 'd' :
	case 'i' :
	case 'o' :
	case 'u' :
	case 'x' :
	    if ((format - bufformat + 1) > (int)sizeof(tformat))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    bytes += cupsFilePrintf(fp, tformat, va_arg(ap, int));
	    break;
	    
	case 'p' : // Pointer value
	    if ((format - bufformat + 1) > (int)sizeof(tformat))
	      break;

	    strncpy(tformat, bufformat, format - bufformat);
	    tformat[format - bufformat] = '\0';

	    bytes += cupsFilePrintf(fp, tformat, va_arg(ap, void *));
	    break;

        case 'c' : // Character or character array
	    if (width <= 1)
	    {
	      bytes ++;
	      cupsFilePutChar(fp, va_arg(ap, int));
	    }
	    else
	    {
	      cupsFileWrite(fp, va_arg(ap, char *), width);
	      bytes += width;
	    }
	    break;

	case 's' : // String
	    if ((s = va_arg(ap, char *)) == NULL)
	      s = (char *)"(nil)";

	    slen = strlen(s);
	    if (slen > width && prec != width)
	      width = slen;

            if (slen > width)
	      slen = width;

            if (sign != '-')
	    {
	      for (i = width - slen; i > 0; i --, bytes ++)
	        cupsFilePutChar(fp, ' ');
	    }

            for (i = slen; i > 0; i --, s ++, bytes ++)
	    {
	      if (*s == '\\' || *s == '\"')
	      {
	        cupsFilePutChar(fp, '\\');
		bytes ++;
	      }

	      cupsFilePutChar(fp, *s);
	    }

            if (sign == '-')
	    {
	      for (i = width - slen; i > 0; i --, bytes ++)
	        cupsFilePutChar(fp, ' ');
	    }
	    break;
      }
    }
    else
    {
      cupsFilePutChar(fp, *format++);
      bytes ++;
    }
  }

  va_end(ap);

  // Return the number of characters written.
  return (bytes);
}


//
// 'ppdcSource::read_file()' - Read a driver source file.
//

void
ppdcSource::read_file(const char *f)	// I - File to read
{
  ppdcFile *fp = new ppdcFile(f);
  scan_file(fp);
  delete fp;
}


//
// 'ppdcSource::scan_file()' - Scan a driver source file.
//

void
ppdcSource::scan_file(ppdcFile   *fp,	// I - File to read
                      ppdcDriver *td,	// I - Driver template
		      bool       inc)	// I - Including?
{
  ppdcDriver	*d;			// Current driver
  ppdcGroup	*g,			// Current group
		*general,		// General options group
		*install;		// Installable options group
  ppdcOption	*o;			// Current option
  ppdcChoice	*c;			// Current choice
  char		temp[256],		// Token from file...
		*ptr;			// Pointer into token
  int		isdefault;		// Default option?


  // Initialize things as needed...
  if (inc && td)
    d = td;
  else
    d = new ppdcDriver(td);

  if ((general = d->find_group("General")) == NULL)
  {
    general = new ppdcGroup("General", NULL);
    d->add_group(general);
  }

  if ((install = d->find_group("InstallableOptions")) == NULL)
  {
    install = new ppdcGroup("InstallableOptions", "Installable Options");
    d->add_group(install);
  }

  // Loop until EOF or }
  o = 0;
  g = general;
  while (get_token(fp, temp, sizeof(temp)))
  {
    if (temp[0] == '*')
    {
      // Mark the next choice as the default
      isdefault = 1;

      for (ptr = temp; ptr[1]; ptr ++)
        *ptr = ptr[1];

      *ptr = '\0';
    }
    else
    {
      // Don't mark the next choice as the default
      isdefault = 0;
    }

    if (!strcasecmp(temp, "}"))
    {
      // Close this one out...
      break;
    }
    else if (!strcasecmp(temp, "{"))
    {
      // Open a new child...
      scan_file(fp, d);
    }
    else if (!strcasecmp(temp, "#define"))
    {
      // Get the variable...
      get_variable(fp);
    }
    else if (!strcasecmp(temp, "#include"))
    {
      // #include filename
      char	basedir[1024],		// Base directory
		*baseptr,		// Pointer into directory
		inctemp[1024],		// Initial filename
		incname[1024];		// Include filename
      ppdcFile	*incfile;		// Include file


      // Get the include name...
      if (!get_token(fp, inctemp, sizeof(inctemp)))
      {
        fprintf(stderr, "ppdc: Expected include filename on line %d of %s!\n",
	        fp->line, fp->filename);
        break;
      }

      // Figure out the current directory...
      strlcpy(basedir, fp->filename, sizeof(basedir));

      if ((baseptr = strrchr(basedir, '/')) != NULL)
        *baseptr = '\0';
      else
        strcpy(basedir, ".");

      // Find the include file...
      if (find_include(inctemp, basedir, incname, sizeof(incname)))
      {
	// Open the include file, scan it, and then close it...
	incfile = new ppdcFile(incname);
	scan_file(incfile, d, true);
	delete incfile;
      }
      else
      {
        // Can't find it!
	fprintf(stderr,
	        "ppdc: Unable to find include file \"%s\" on line %d of %s!\n",
	        inctemp, fp->line, fp->filename);
	break;
      }
    }
    else if (!strcasecmp(temp, "#media"))
    {
      ppdcMediaSize	*m;		// Media size


      // Get a media size...
      m = get_size(fp);
      if (m)
        sizes->add(m);
    }
    else if (!strcasecmp(temp, "#po"))
    {
      ppdcCatalog	*cat;		// Message catalog


      // Get a message catalog...
      cat = get_po(fp);
      if (cat)
        po_files->add(cat);
    }
    else if (!strcasecmp(temp, "Attribute"))
    {
      ppdcAttr	*a;			// Attribute


      // Get an attribute...
      a = get_attr(fp);
      if (a)
        d->add_attr(a);
    }
    else if (!strcasecmp(temp, "Choice"))
    {
      // Get a choice...
      if (!o)
      {
        fprintf(stderr, "ppdc: Choice found on line %d of %s with no Option!\n",
	        fp->line, fp->filename);
        break;
      }

      c = get_choice(fp);
      if (!c)
        break;

      // Add it to the current option...
      o->add_choice(c);

      if (isdefault)
        o->set_defchoice(c);
    }
    else if (!strcasecmp(temp, "ColorDevice"))
    {
      // ColorDevice boolean
      d->color_device = get_boolean(fp);
    }
    else if (!strcasecmp(temp, "ColorModel"))
    {
      // Get the color model
      c = get_color_model(fp);
      if (!c)
        continue;

      // Add the choice to the ColorModel option...
      if ((o = d->find_option("ColorModel")) == NULL)
      {
	// Create the ColorModel option...
	o = new ppdcOption(PPDC_PICKONE, "ColorModel", "Color Mode", PPDC_SECTION_ANY, 10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "ColorProfile"))
    {
      ppdcProfile	*p;		// Color profile


      // Get the color profile...
      p = get_color_profile(fp);

      if (p)
        d->profiles->add(p);
    }
    else if (!strcasecmp(temp, "Copyright"))
    {
      // Copyright string
      char	copytemp[8192],		// Copyright string
		*copyptr,		// Pointer into string
		*copyend;		// Pointer to end of string


      // Get the copyright string...
      if (!get_token(fp, copytemp, sizeof(temp)))
      {
        fprintf(stderr,
	        "ppdc: Expected string after Copyright on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      // Break it up into individual lines...
      for (copyptr = copytemp; copyptr; copyptr = copyend)
      {
        if ((copyend = strchr(copyptr, '\n')) != NULL)
	  *copyend++ = '\0';

        d->copyright->add(new ppdcString(copyptr));
      }
    }
    else if (!strcasecmp(temp, "CustomMedia"))
    {
      ppdcMediaSize	*m;		// Media size


      // Get a custom media size...
      m = get_custom_size(fp);
      if (m)
        d->sizes->add(m);

      if (isdefault)
        d->set_default_size(m);
    }
    else if (!strcasecmp(temp, "Cutter"))
    {
      // Cutter boolean
      int	have_cutter;		// Have a paper cutter?


      have_cutter = get_boolean(fp);
      if (have_cutter <= 0)
        continue;

      if ((o = d->find_option("CutMedia")) == NULL)
      {
        o = new ppdcOption(PPDC_BOOLEAN, "CutMedia", "Cut Media", PPDC_SECTION_ANY, 10.0f);

	g = general;
	g->add_option(o);

	c = new ppdcChoice("False", NULL, "<</CutMedia 0>>setpagedevice");
	o->add_choice(c);
	o->set_defchoice(c);

	c = new ppdcChoice("True", NULL, "<</CutMedia 4>>setpagedevice");
	o->add_choice(c);
      }

      o = NULL;
    }
    else if (!strcasecmp(temp, "Darkness"))
    {
      // Get the darkness choice...
      c = get_generic(fp, "Darkness", NULL, "cupsCompression");
      if (!c)
        continue;

      // Add the choice to the cupsDarkness option...
      if ((o = d->find_option("cupsDarkness")) == NULL)
      {
	// Create the cupsDarkness option...
	o = new ppdcOption(PPDC_PICKONE, "cupsDarkness", "Darkness", PPDC_SECTION_ANY, 10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "DriverType"))
    {
      int	i;			// Looping var


      // DriverType keyword
      if (!get_token(fp, temp, sizeof(temp)))
      {
        fprintf(stderr, "ppdc: Expected driver type keyword following DriverType on line %d of %s!\n",
	        fp->line, fp->filename);
        continue;
      }

      for (i = 0; i < (int)(sizeof(driver_types) / sizeof(driver_types[0])); i ++)
        if (!strcasecmp(temp, driver_types[i]))
	  break;

      if (i < (int)(sizeof(driver_types) / sizeof(driver_types[0])))
        d->type = (ppdcDrvType)i;
      else if (!strcasecmp(temp, "dymo"))
        d->type = PPDC_DRIVER_LABEL;
      else
        fprintf(stderr, "ppdc: Unknown driver type %s on line %d of %s!\n",
	        temp, fp->line, fp->filename);
    }
    else if (!strcasecmp(temp, "Duplex"))
      get_duplex(fp, d);
    else if (!strcasecmp(temp, "Filter"))
    {
      ppdcFilter	*f;		// Filter


      // Get the filter value...
      f = get_filter(fp);
      if (f)
        d->filters->add(f);
    }
    else if (!strcasecmp(temp, "Finishing"))
    {
      // Get the finishing choice...
      c = get_generic(fp, "Finishing", "OutputType", NULL);
      if (!c)
        continue;

      // Add the choice to the cupsFinishing option...
      if ((o = d->find_option("cupsFinishing")) == NULL)
      {
	// Create the cupsFinishing option...
	o = new ppdcOption(PPDC_PICKONE, "cupsFinishing", "Finishing", PPDC_SECTION_ANY, 10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "Font") ||
             !strcasecmp(temp, "#font"))
    {
      ppdcFont	*f;			// Font


      // Get a font...
      f = get_font(fp);
      if (f)
      {
	if (!strcasecmp(temp, "#font"))
	  base_fonts->add(f);
        else
	  d->add_font(f);

        if (isdefault)
          d->set_default_font(f);
      }
    }
    else if (!strcasecmp(temp, "Group"))
    {
      // Get a group...
      g = get_group(fp, d);
      if (!g)
        break;
    }
    else if (!strcasecmp(temp, "HWMargins"))
    {
      // HWMargins left bottom right top
      d->left_margin   = get_measurement(fp);
      d->bottom_margin = get_measurement(fp);
      d->right_margin  = get_measurement(fp);
      d->top_margin    = get_measurement(fp);
    }
    else if (!strcasecmp(temp, "InputSlot"))
    {
      // Get the input slot choice...
      c = get_generic(fp, "InputSlot", NULL, "MediaPosition");
      if (!c)
        continue;

      // Add the choice to the InputSlot option...
      if ((o = d->find_option("InputSlot")) == NULL)
      {
	// Create the InputSlot option...
	o = new ppdcOption(PPDC_PICKONE, "InputSlot", "Media Source",
	                   PPDC_SECTION_ANY, 10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "Installable"))
    {
      // Get the installable option...
      o = get_installable(fp);

      // Add it as needed...
      if (o)
      {
        install->add_option(o);
        o = NULL;
      }
    }
    else if (!strcasecmp(temp, "ManualCopies"))
    {
      // ManualCopies boolean
      d->manual_copies = get_boolean(fp);
    }
    else if (!strcasecmp(temp, "Manufacturer"))
    {
      // Manufacturer name
      char	name[256];		// Model name string


      if (!get_token(fp, name, sizeof(name)))
      {
        fprintf(stderr, "ppdc: Expected name after Manufacturer on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      d->set_manufacturer(name);
    }
    else if (!strcasecmp(temp, "MaxSize"))
    {
      // MaxSize width length
      d->max_width  = get_measurement(fp);
      d->max_length = get_measurement(fp);
    }
    else if (!strcasecmp(temp, "MediaSize"))
    {
      // MediaSize keyword
      char		name[41];	// Media size name
      ppdcMediaSize	*m,		// Matching media size...
			*dm;		// Driver media size...


      if (get_token(fp, name, sizeof(name)) == NULL)
      {
        fprintf(stderr,
	        "ppdc: Expected name after MediaSize on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      m = find_size(name);

      if (!m)
      {
        fprintf(stderr, "ppdc: Unknown media size \"%s\" on line %d of %s!\n",
	        name, fp->line, fp->filename);
	break;
      }

      // Add this size to the driver...
      dm = new ppdcMediaSize(m->name->value, m->text->value,
                             m->width, m->length, d->left_margin,
			     d->bottom_margin, d->right_margin,
			     d->top_margin);
      d->sizes->add(dm);

      if (isdefault)
        d->set_default_size(dm);
    }
    else if (!strcasecmp(temp, "MediaType"))
    {
      // Get the media type choice...
      c = get_generic(fp, "MediaType", "MediaType", "cupsMediaType");
      if (!c)
        continue;

      // Add the choice to the MediaType option...
      if ((o = d->find_option("MediaType")) == NULL)
      {
	// Create the MediaType option...
	o = new ppdcOption(PPDC_PICKONE, "MediaType", "Media Type",
	                   PPDC_SECTION_ANY, 10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "MinSize"))
    {
      // MinSize width length
      d->min_width  = get_measurement(fp);
      d->min_length = get_measurement(fp);
    }
    else if (!strcasecmp(temp, "ModelName"))
    {
      // ModelName name
      char	name[256];		// Model name string


      if (!get_token(fp, name, sizeof(name)))
      {
        fprintf(stderr, "ppdc: Expected name after ModelName on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      d->set_model_name(name);
    }
    else if (!strcasecmp(temp, "ModelNumber"))
    {
      // ModelNumber number
      d->model_number = get_integer(fp);
    }
    else if (!strcasecmp(temp, "Option"))
    {
      // Get an option...
      o = get_option(fp, d, g);
      if (!o)
        break;
    }
    else if (!strcasecmp(temp, "FileName"))
    {
      // FileName name
      char	name[256];		// Filename string


      if (!get_token(fp, name, sizeof(name)))
      {
        fprintf(stderr,
	        "ppdc: Expected name after FileName on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      d->set_file_name(name);
    }
    else if (!strcasecmp(temp, "PCFileName"))
    {
      // PCFileName name
      char	name[256];		// PC filename string


      if (!get_token(fp, name, sizeof(name)))
      {
        fprintf(stderr,
	        "ppdc: Expected name after PCFileName on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      d->set_pc_file_name(name);
    }
    else if (!strcasecmp(temp, "Resolution"))
    {
      // Get the resolution choice...
      c = get_resolution(fp);
      if (!c)
        continue;

      // Add the choice to the Resolution option...
      if ((o = d->find_option("Resolution")) == NULL)
      {
	// Create the Resolution option...
	o = new ppdcOption(PPDC_PICKONE, "Resolution", NULL, PPDC_SECTION_ANY,
	                   10.0f);
	g = general;
	g->add_option(o);
      }

      o->add_choice(c);

      if (isdefault)
	o->set_defchoice(c);

      o = NULL;
    }
    else if (!strcasecmp(temp, "SimpleColorProfile"))
    {
      ppdcProfile	*p;		// Color profile


      // Get the color profile...
      p = get_simple_profile(fp);

      if (p)
        d->profiles->add(p);
    }
    else if (!strcasecmp(temp, "Throughput"))
    {
      // Throughput number
      d->throughput = get_integer(fp);
    }
    else if (!strcasecmp(temp, "UIConstraints"))
    {
      ppdcConstraint	*con;		// Constraint


      con = get_constraint(fp);

      if (con)
        d->constraints->add(con);
    }
    else if (!strcasecmp(temp, "VariablePaperSize"))
    {
      // VariablePaperSize boolean
      d->variable_paper_size = get_boolean(fp);
    }
    else if (!strcasecmp(temp, "Version"))
    {
      // Version string
      char	name[256];		// Model name string


      if (!get_token(fp, name, sizeof(name)))
      {
        fprintf(stderr,
	        "ppdc: Expected string after Version on line %d of %s!\n",
	        fp->line, fp->filename);
	break;
      }

      d->set_version(name);
    }
    else
    {
      fprintf(stderr, "ppdc: Unknown token \"%s\" seen on line %d of %s!\n",
              temp, fp->line, fp->filename);
      break;
    }
  }

  // Done processing this block, is there anything to save?
  if (!inc)
  {
    if (!d->pc_file_name || !d->model_name || !d->manufacturer || !d->version ||
	!d->sizes->count)
    {
      // Nothing to save...
      d->release();
    }
    else
    {
      // Got a driver, save it...
      drivers->add(d);
    }
  }
}


//
// 'ppdcSource::set_variable()' - Set a variable.
//

ppdcVariable *				// O - Variable
ppdcSource::set_variable(
    const char *name,			// I - Name
    const char *value)			// I - Value
{
  ppdcVariable	*v;			// Variable


  // See if the variable exists already...
  v = find_variable(name);
  if (v)
  {
    // Change the variable value...
    v->set_value(value);
  }
  else
  {
    // Create a new variable and add it...
    v = new ppdcVariable(name, value);
    vars->add(v);
  }

  return (v);
}


//
// 'ppdcSource::write_file()' - Write the current source data to a file.
//

int					// O - 0 on success, -1 on error
ppdcSource::write_file(const char *f)	// I - File to write
{
  cups_file_t	*fp;			// Output file
  char		bckname[1024];		// Backup file
  ppdcDriver	*d;			// Current driver
  ppdcString	*st;			// Current string
  ppdcAttr	*a;			// Current attribute
  ppdcConstraint *co;			// Current constraint
  ppdcFilter	*fi;			// Current filter
  ppdcFont	*fo;			// Current font
  ppdcGroup	*g;			// Current group
  ppdcOption	*o;			// Current option
  ppdcChoice	*ch;			// Current choice
  ppdcProfile	*p;			// Current color profile
  ppdcMediaSize	*si;			// Current media size
  float		left,			// Current left margin
		bottom,			// Current bottom margin
		right,			// Current right margin
		top;			// Current top margin
  int		dtused[PPDC_DRIVER_MAX];// Driver type usage...


  // Rename the current file, if any, to .bck...
  snprintf(bckname, sizeof(bckname), "%s.bck", f);
  rename(f, bckname);

  // Open the output file...
  fp = cupsFileOpen(f, "w");

  if (!fp)
  {
    // Can't create file; restore backup and return...
    rename(bckname, f);
    return (-1);
  }

  cupsFilePuts(fp, "// CUPS PPD Compiler " CUPS_SVERSION "\n\n");

  // Include standard files...
  cupsFilePuts(fp, "// Include necessary files...\n");
  cupsFilePuts(fp, "#include <font.defs>\n");
  cupsFilePuts(fp, "#include <media.defs>\n");

  memset(dtused, 0, sizeof(dtused));

  for (d = (ppdcDriver *)drivers->first(); d; d = (ppdcDriver *)drivers->next())
    if (d->type > PPDC_DRIVER_PS && !dtused[d->type])
    {
      cupsFilePrintf(fp, "#include <%s.h>\n", driver_types[d->type]);
      dtused[d->type] = 1;
    }

  // Output each driver...
  for (d = (ppdcDriver *)drivers->first(); d; d = (ppdcDriver *)drivers->next())
  {
    // Start the driver...
    cupsFilePrintf(fp, "\n// %s %s\n", d->manufacturer->value, d->model_name->value);
    cupsFilePuts(fp, "{\n");

    // Write the copyright stings...
    for (st = (ppdcString *)d->copyright->first();
         st;
	 st = (ppdcString *)d->copyright->next())
      quotef(fp, "  Copyright \"%s\"\n", st->value);

    // Write other strings and values...
    if (d->manufacturer->value)
      quotef(fp, "  Manufacturer \"%s\"\n", d->manufacturer->value);
    if (d->model_name->value)
      quotef(fp, "  ModelName \"%s\"\n", d->model_name->value);
    if (d->file_name->value)
      quotef(fp, "  FileName \"%s\"\n", d->file_name->value);
    if (d->pc_file_name->value)
      quotef(fp, "  PCFileName \"%s\"\n", d->pc_file_name->value);
    if (d->version->value)
      quotef(fp, "  Version \"%s\"\n", d->version->value);

    cupsFilePrintf(fp, "  DriverType %s\n", driver_types[d->type]);

    if (d->model_number)
    {
      switch (d->type)
      {
        case PPDC_DRIVER_ESCP :
	    cupsFilePuts(fp, "  ModelNumber (");

	    if (d->model_number & ESCP_DOTMATRIX)
	      cupsFilePuts(fp, " $ESCP_DOTMATRIX");
	    if (d->model_number & ESCP_MICROWEAVE)
	      cupsFilePuts(fp, " $ESCP_MICROWEAVE");
	    if (d->model_number & ESCP_STAGGER)
	      cupsFilePuts(fp, " $ESCP_STAGGER");
	    if (d->model_number & ESCP_ESCK)
	      cupsFilePuts(fp, " $ESCP_ESCK");
	    if (d->model_number & ESCP_EXT_UNITS)
	      cupsFilePuts(fp, " $ESCP_EXT_UNITS");
	    if (d->model_number & ESCP_EXT_MARGINS)
	      cupsFilePuts(fp, " $ESCP_EXT_MARGINS");
	    if (d->model_number & ESCP_USB)
	      cupsFilePuts(fp, " $ESCP_USB");
	    if (d->model_number & ESCP_PAGE_SIZE)
	      cupsFilePuts(fp, " $ESCP_PAGE_SIZE");
	    if (d->model_number & ESCP_RASTER_ESCI)
	      cupsFilePuts(fp, " $ESCP_RASTER_ESCI");
	    if (d->model_number & ESCP_REMOTE)
	      cupsFilePuts(fp, " $ESCP_REMOTE");

	    cupsFilePuts(fp, ")\n");
	    break;

	case PPDC_DRIVER_PCL :
	    cupsFilePuts(fp, "  ModelNumber (");

	    if (d->model_number & PCL_PAPER_SIZE)
	      cupsFilePuts(fp, " $PCL_PAPER_SIZE");
	    if (d->model_number & PCL_INKJET)
	      cupsFilePuts(fp, " $PCL_INKJET");
	    if (d->model_number & PCL_RASTER_END_COLOR)
	      cupsFilePuts(fp, " $PCL_RASTER_END_COLOR");
	    if (d->model_number & PCL_RASTER_CID)
	      cupsFilePuts(fp, " $PCL_RASTER_CID");
	    if (d->model_number & PCL_RASTER_CRD)
	      cupsFilePuts(fp, " $PCL_RASTER_CRD");
	    if (d->model_number & PCL_RASTER_SIMPLE)
	      cupsFilePuts(fp, " $PCL_RASTER_SIMPLE");
	    if (d->model_number & PCL_RASTER_RGB24)
	      cupsFilePuts(fp, " $PCL_RASTER_RGB24");
	    if (d->model_number & PCL_PJL)
	      cupsFilePuts(fp, " $PCL_PJL");
	    if (d->model_number & PCL_PJL_PAPERWIDTH)
	      cupsFilePuts(fp, " $PCL_PJL_PAPERWIDTH");
	    if (d->model_number & PCL_PJL_HPGL2)
	      cupsFilePuts(fp, " $PCL_PJL_HPGL2");
	    if (d->model_number & PCL_PJL_PCL3GUI)
	      cupsFilePuts(fp, " $PCL_PJL_PCL3GUI");
	    if (d->model_number & PCL_PJL_RESOLUTION)
	      cupsFilePuts(fp, " $PCL_PJL_RESOLUTION");

	    cupsFilePuts(fp, ")\n");
	    break;

	case PPDC_DRIVER_LABEL :
	    cupsFilePuts(fp, "  ModelNumber ");

	    switch (d->model_number)
	    {
	      case DYMO_3x0 :
		  cupsFilePuts(fp, "$DYMO_3x0\n");
		  break;

	      case ZEBRA_EPL_LINE :
		  cupsFilePuts(fp, "$ZEBRA_EPL_LINE\n");
		  break;

	      case ZEBRA_EPL_PAGE :
		  cupsFilePuts(fp, "$ZEBRA_EPL_PAGE\n");
		  break;

	      case ZEBRA_ZPL :
		  cupsFilePuts(fp, "$ZEBRA_ZPL\n");
		  break;

	      case ZEBRA_CPCL :
		  cupsFilePuts(fp, "$ZEBRA_CPCL\n");
		  break;

	      case INTELLITECH_PCL :
		  cupsFilePuts(fp, "$INTELLITECH_PCL\n");
		  break;

	      default :
		  cupsFilePrintf(fp, "%d\n", d->model_number);
		  break;
	    }
	    break;

	case PPDC_DRIVER_EPSON :
	    cupsFilePuts(fp, "  ModelNumber ");

	    switch (d->model_number)
	    {
	      case EPSON_9PIN :
		  cupsFilePuts(fp, "$EPSON_9PIN\n");
		  break;

	      case EPSON_24PIN :
		  cupsFilePuts(fp, "$EPSON_24PIN\n");
		  break;

	      case EPSON_COLOR :
		  cupsFilePuts(fp, "$EPSON_COLOR\n");
		  break;

	      case EPSON_PHOTO :
		  cupsFilePuts(fp, "$EPSON_PHOTO\n");
		  break;

	      case EPSON_ICOLOR :
		  cupsFilePuts(fp, "$EPSON_ICOLOR\n");
		  break;

	      case EPSON_IPHOTO :
		  cupsFilePuts(fp, "$EPSON_IPHOTO\n");
		  break;

	      default :
		  cupsFilePrintf(fp, "%d\n", d->model_number);
	          break;
	    }
	    break;

	case PPDC_DRIVER_HP :
	    cupsFilePuts(fp, "  ModelNumber ");
	    switch (d->model_number)
	    {
	      case HP_LASERJET :
	          cupsFilePuts(fp, "$HP_LASERJET\n");
		  break;

	      case HP_DESKJET :
	          cupsFilePuts(fp, "$HP_DESKJET\n");
		  break;

	      case HP_DESKJET2 :
	          cupsFilePuts(fp, "$HP_DESKJET2\n");
		  break;

	      default :
		  cupsFilePrintf(fp, "%d\n", d->model_number);
		  break;
	    }

	    cupsFilePuts(fp, ")\n");
	    break;

        default :
            cupsFilePrintf(fp, "  ModelNumber %d\n", d->model_number);
	    break;
      }
    }

    if (d->manual_copies)
      cupsFilePuts(fp, "  ManualCopies Yes\n");

    if (d->color_device)
      cupsFilePuts(fp, "  ColorDevice Yes\n");

    if (d->throughput)
      cupsFilePrintf(fp, "  Throughput %d\n", d->throughput);

    // Output all of the attributes...
    for (a = (ppdcAttr *)d->attrs->first();
         a;
	 a = (ppdcAttr *)d->attrs->next())
      if (a->text->value && a->text->value[0])
	quotef(fp, "  Attribute \"%s\" \"%s/%s\" \"%s\"\n",
               a->name->value, a->selector->value ? a->selector->value : "",
	       a->text->value, a->value->value ? a->value->value : "");
      else
	quotef(fp, "  Attribute \"%s\" \"%s\" \"%s\"\n",
               a->name->value, a->selector->value ? a->selector->value : "",
	       a->value->value ? a->value->value : "");

    // Output all of the constraints...
    for (co = (ppdcConstraint *)d->constraints->first();
         co;
	 co = (ppdcConstraint *)d->constraints->next())
    {
      if (co->option1->value[0] == '*')
	cupsFilePrintf(fp, "  UIConstraints \"%s %s", co->option1->value,
		       co->choice1->value ? co->choice1->value : "");
      else
	cupsFilePrintf(fp, "  UIConstraints \"*%s %s", co->option1->value,
		       co->choice1->value ? co->choice1->value : "");

      if (co->option2->value[0] == '*')
	cupsFilePrintf(fp, " %s %s\"\n", co->option2->value,
		       co->choice2->value ? co->choice2->value : "");
      else
	cupsFilePrintf(fp, " *%s %s\"\n", co->option2->value,
		       co->choice2->value ? co->choice2->value : "");
    }

    // Output all of the filters...
    for (fi = (ppdcFilter *)d->filters->first();
         fi;
	 fi = (ppdcFilter *)d->filters->next())
      cupsFilePrintf(fp, "  Filter \"%s %d %s\"\n",
                     fi->mime_type->value, fi->cost, fi->program->value);

    // Output all of the fonts...
    for (fo = (ppdcFont *)d->fonts->first();
         fo;
	 fo = (ppdcFont *)d->fonts->next())
      if (!strcmp(fo->name->value, "*"))
        cupsFilePuts(fp, "  Font *\n");
      else
	cupsFilePrintf(fp, "  Font \"%s\" \"%s\" \"%s\" \"%s\" %s\n",
        	       fo->name->value, fo->encoding->value,
		       fo->version->value, fo->charset->value,
		       fo->status == PPDC_FONT_ROM ? "ROM" : "Disk");

    // Output all options...
    for (g = (ppdcGroup *)d->groups->first();
         g;
	 g = (ppdcGroup *)d->groups->next())
    {
      if (g->options->count == 0)
        continue;

      if (g->text->value && g->text->value[0])
        quotef(fp, "  Group \"%s/%s\"\n", g->name->value, g->text->value);
      else
        cupsFilePrintf(fp, "  Group \"%s\"\n", g->name->value);

      for (o = (ppdcOption *)g->options->first();
           o;
	   o = (ppdcOption *)g->options->next())
      {
        if (o->choices->count == 0)
	  continue;

	if (o->text->value && o->text->value[0])
          quotef(fp, "    Option \"%s/%s\"", o->name->value, o->text->value);
	else
          cupsFilePrintf(fp, "    Option \"%s\"", o->name->value);

        cupsFilePrintf(fp, " %s %s %.1f\n",
		       o->type == PPDC_BOOLEAN ? "Boolean" :
			   o->type == PPDC_PICKONE ? "PickOne" : "PickMany",
		       o->section == PPDC_SECTION_ANY ? "AnySetup" :
			   o->section == PPDC_SECTION_DOCUMENT ? "DocumentSetup" :
			   o->section == PPDC_SECTION_EXIT ? "ExitServer" :
			   o->section == PPDC_SECTION_JCL ? "JCLSetup" :
			   o->section == PPDC_SECTION_PAGE ? "PageSetup" :
			   "Prolog",
		       o->order);

        for (ch = (ppdcChoice *)o->choices->first();
	     ch;
	     ch = (ppdcChoice *)o->choices->next())
	{
	  if (ch->text->value && ch->text->value[0])
            quotef(fp, "      %sChoice \"%s/%s\" \"%s\"\n",
	    	   o->defchoice == ch->name ? "*" : "",
                   ch->name->value, ch->text->value,
		   ch->code->value ? ch->code->value : "");
	  else
            quotef(fp, "      %sChoice \"%s\" \"%s\"\n",
	           o->defchoice == ch->name ? "*" : "",
		   ch->name->value,
		   ch->code->value ? ch->code->value : "");
	}
      }
    }

    // Output all of the color profiles...
    for (p = (ppdcProfile *)d->profiles->first();
         p;
	 p = (ppdcProfile *)d->profiles->next())
      cupsFilePrintf(fp, "  ColorProfile \"%s/%s\" %.3f %.3f "
                	 "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
        	     p->resolution->value, p->media_type->value,
		     p->density, p->gamma,
		     p->profile[0], p->profile[1], p->profile[2],
		     p->profile[3], p->profile[4], p->profile[5],
		     p->profile[6], p->profile[7], p->profile[8]);

    // Output all of the media sizes...
    left   = 0.0;
    bottom = 0.0;
    right  = 0.0;
    top    = 0.0;

    for (si = (ppdcMediaSize *)d->sizes->first();
         si;
	 si = (ppdcMediaSize *)d->sizes->next())
      if (si->size_code->value && si->region_code->value)
      {
        // Output a custom media size...
	quotef(fp, "  %sCustomMedia \"%s/%s\" %.2f %.2f %.2f %.2f %.2f %.2f \"%s\" \"%s\"\n",
	       si->name == d->default_size ? "*" : "", si->name->value,
	       si->text->value, si->width, si->length, si->left, si->bottom,
	       si->right, si->top, si->size_code->value,
	       si->region_code->value);
      }
      else
      {
        // Output a standard media size...
	if (fabs(left - si->left) > 0.1 ||
            fabs(bottom - si->bottom) > 0.1 ||
            fabs(right - si->right) > 0.1 ||
            fabs(top - si->top) > 0.1)
	{
          cupsFilePrintf(fp, "  HWMargins %.2f %.2f %.2f %.2f\n",
	        	 si->left, si->bottom, si->right, si->top);

          left   = si->left;
	  bottom = si->bottom;
	  right  = si->right;
	  top    = si->top;
	}

	cupsFilePrintf(fp, "  %sMediaSize %s\n",
	               si->name == d->default_size ? "*" : "",
        	       si->name->value);
      }

    if (d->variable_paper_size)
    {
      cupsFilePuts(fp, "  VariablePaperSize Yes\n");

      if (fabs(left - d->left_margin) > 0.1 ||
          fabs(bottom - d->bottom_margin) > 0.1 ||
          fabs(right - d->right_margin) > 0.1 ||
          fabs(top - d->top_margin) > 0.1)
      {
        cupsFilePrintf(fp, "  HWMargins %.2f %.2f %.2f %.2f\n",
	               d->left_margin, d->bottom_margin, d->right_margin,
		       d->top_margin);
      }

      cupsFilePrintf(fp, "  MinSize %.2f %.2f\n", d->min_width, d->min_length);
      cupsFilePrintf(fp, "  MaxSize %.2f %.2f\n", d->max_width, d->max_length);
    }

    // End the driver...
    cupsFilePuts(fp, "}\n");
  }

  // Close the file and return...
  cupsFileClose(fp);

  return (0);
}


//
// End of "$Id$".
//
