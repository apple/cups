/*
 * "$Id$"
 *
 *   PostScript Printer Description definitions for the Common UNIX Printing
 *   System (CUPS).
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
 *   This code and any derivative of it may be used and distributed
 *   freely under the terms of the GNU General Public License when
 *   used with GNU Ghostscript or its derivatives.  Use of the code
 *   (or any derivative of it) with software other than GNU
 *   GhostScript (or its derivatives) is governed by the CUPS license
 *   agreement.
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

#ifndef _CUPS_PPD_H_
#  define _CUPS_PPD_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>
#  include "file.h"


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * PPD version...
 */

#  define PPD_VERSION	4.3	/* Kept in sync with Adobe version number */


/*
 * PPD size limits (defined in Adobe spec)
 */

#  define PPD_MAX_NAME	41	/* Maximum size of name + 1 for nul */
#  define PPD_MAX_TEXT	81	/* Maximum size of text + 1 for nul */
#  define PPD_MAX_LINE	256	/* Maximum size of line + 1 for nul */


/*
 * Types and structures...
 */

typedef enum ppd_ui_e		/**** UI Types ****/
{
  PPD_UI_BOOLEAN,		/* True or False option */
  PPD_UI_PICKONE,		/* Pick one from a list */
  PPD_UI_PICKMANY		/* Pick zero or more from a list */
} ppd_ui_t;

typedef enum ppd_section_e	/**** Order dependency sections ****/
{
  PPD_ORDER_ANY,		/* Option code can be anywhere in the file */
  PPD_ORDER_DOCUMENT,		/* ... must be in the DocumentSetup section */
  PPD_ORDER_EXIT,		/* ... must be sent prior to the document */
  PPD_ORDER_JCL,		/* ... must be sent as a JCL command */
  PPD_ORDER_PAGE,		/* ... must be in the PageSetup section */
  PPD_ORDER_PROLOG		/* ... must be in the Prolog section */
} ppd_section_t;

typedef enum ppd_cs_e		/**** Colorspaces ****/
{
  PPD_CS_CMYK = -4,		/* CMYK colorspace */
  PPD_CS_CMY,			/* CMY colorspace */
  PPD_CS_GRAY = 1,		/* Grayscale colorspace */
  PPD_CS_RGB = 3,		/* RGB colorspace */
  PPD_CS_RGBK,			/* RGBK (K = gray) colorspace */
  PPD_CS_N			/* DeviceN colorspace */
} ppd_cs_t;

typedef enum ppd_status_e	/**** Status Codes ****/
{
  PPD_OK = 0,			/* OK */
  PPD_FILE_OPEN_ERROR,		/* Unable to open PPD file */
  PPD_NULL_FILE,		/* NULL PPD file pointer */
  PPD_ALLOC_ERROR,		/* Memory allocation error */
  PPD_MISSING_PPDADOBE4,	/* Missing PPD-Adobe-4.x header */
  PPD_MISSING_VALUE,		/* Missing value string */
  PPD_INTERNAL_ERROR,		/* Internal error */
  PPD_BAD_OPEN_GROUP,		/* Bad OpenGroup */
  PPD_NESTED_OPEN_GROUP,	/* OpenGroup without a CloseGroup first */
  PPD_BAD_OPEN_UI,		/* Bad OpenUI/JCLOpenUI */
  PPD_NESTED_OPEN_UI,		/* OpenUI/JCLOpenUI without a CloseUI/JCLCloseUI first */
  PPD_BAD_ORDER_DEPENDENCY,	/* Bad OrderDependency */
  PPD_BAD_UI_CONSTRAINTS,	/* Bad UIConstraints */
  PPD_MISSING_ASTERISK,		/* Missing asterisk in column 0 */
  PPD_LINE_TOO_LONG,		/* Line longer than 255 chars */
  PPD_ILLEGAL_CHARACTER,	/* Illegal control character */
  PPD_ILLEGAL_MAIN_KEYWORD,	/* Illegal main keyword string */
  PPD_ILLEGAL_OPTION_KEYWORD,	/* Illegal option keyword string */
  PPD_ILLEGAL_TRANSLATION,	/* Illegal translation string */
  PPD_ILLEGAL_WHITESPACE	/* Illegal whitespace character */
} ppd_status_t;

typedef enum ppd_conform_e	/**** Conformance Levels ****/
{
  PPD_CONFORM_RELAXED,		/* Relax whitespace and control char */
  PPD_CONFORM_STRICT		/* Require strict conformance */
} ppd_conform_t;

typedef struct ppd_attr_str	/**** PPD Attribute Structure ****/
{
  char		name[PPD_MAX_NAME];
  				/* Name of attribute (cupsXYZ) */
  char		spec[PPD_MAX_NAME];
				/* Specifier string, if any */
  char		text[PPD_MAX_TEXT];
				/* Human-readable text, if any */
  char		*value;		/* Value string */
} ppd_attr_t;

typedef struct ppd_option_str ppd_option_t;
				/**** Options ****/

typedef struct ppd_choice_str	/**** Option choices ****/
{
  char		marked;		/* 0 if not selected, 1 otherwise */
  char		choice[PPD_MAX_NAME];
				/* Computer-readable option name */
  char		text[PPD_MAX_TEXT];
				/* Human-readable option name */
  char		*code;		/* Code to send for this option */
  ppd_option_t	*option;	/* Pointer to parent option structure */
} ppd_choice_t;

struct ppd_option_str		/**** Options ****/
{
  char		conflicted;	/* 0 if no conflicts exist, 1 otherwise */
  char		keyword[PPD_MAX_NAME];
				/* Option keyword name ("PageSize", etc.) */
  char		defchoice[PPD_MAX_NAME];
				/* Default option choice */
  char		text[PPD_MAX_TEXT];
				/* Human-readable text */
  ppd_ui_t	ui;		/* Type of UI option */
  ppd_section_t	section;	/* Section for command */
  float		order;		/* Order number */
  int		num_choices;	/* Number of option choices */
  ppd_choice_t	*choices;	/* Option choices */
};

typedef struct ppd_group_str	/**** Groups ****/
{
  /**** Group text strings are limited to 39 chars + nul in order to
   **** preserve binary compatibility and allow applications to get
   **** the group's keyword name.
   ****/
  char		text[PPD_MAX_TEXT - PPD_MAX_NAME];
  				/* Human-readable group name */
  char		name[PPD_MAX_NAME];
				/* Group name */
  int		num_options;	/* Number of options */
  ppd_option_t	*options;	/* Options */
  int		num_subgroups;	/* Number of sub-groups */
  struct ppd_group_str	*subgroups;
				/* Sub-groups (max depth = 1) */
} ppd_group_t;

typedef struct			/**** Constraints ****/
{
  char		option1[PPD_MAX_NAME];
  				/* First keyword */
  char		choice1[PPD_MAX_NAME];
				/* First option/choice (blank for all) */
  char		option2[PPD_MAX_NAME];
				/* Second keyword */
  char		choice2[PPD_MAX_NAME];
				/* Second option/choice (blank for all) */
} ppd_const_t;

typedef struct ppd_size_str	/**** Page Sizes ****/
{
  int		marked;		/* Page size selected? */
  char		name[PPD_MAX_NAME];
  				/* Media size option */
  float		width;		/* Width of media in points */
  float		length;		/* Length of media in points */
  float		left;		/* Left printable margin in points */
  float		bottom;		/* Bottom printable margin in points */
  float		right;		/* Right printable margin in points */
  float		top;		/* Top printable margin in points */
} ppd_size_t;

typedef struct ppd_emul_str	/**** Emulators ****/
{
  char		name[PPD_MAX_NAME];
  				/* Emulator name */
  char		*start;		/* Code to switch to this emulation */
  char		*stop;		/* Code to stop this emulation */
} ppd_emul_t;

typedef struct ppd_profile_str	/**** sRGB Color Profiles ****/
{
  char		resolution[PPD_MAX_NAME];
  				/* Resolution or "-" */
  char		media_type[PPD_MAX_NAME];
				/* Media type of "-" */
  float		density;	/* Ink density to use */
  float		gamma;		/* Gamma correction to use */
  float		matrix[3][3];	/* Transform matrix */
} ppd_profile_t;

/**** New in CUPS 1.2 ****/
#  if 0
typedef enum ppd_ext_ui_e	/**** Extended UI Types ****/
{
  PPD_UI_CUPS_TEXT,		/* Specify a string */
  PPD_UI_CUPS_INTEGER,		/* Specify an integer number */
  PPD_UI_CUPS_REAL,		/* Specify a real number */
  PPD_UI_CUPS_GAMMA,		/* Specify a gamma number */
  PPD_UI_CUPS_CURVE,		/* Specify start, end, and gamma numbers */
  PPD_UI_CUPS_INTEGER_ARRAY,	/* Specify an array of integer numbers */
  PPD_UI_CUPS_REAL_ARRAY,	/* Specify an array of real numbers */
  PPD_UI_CUPS_XY_ARRAY		/* Specify an array of X/Y real numbers */
} ppd_ext_ui_t;

typedef union ppd_ext_value_u	/**** Extended Values ****/
{
  char		*text;		/* Text value */
  int		integer;	/* Integer value */
  float		real;		/* Real value */
  float		gamma;		/* Gamma value */
  struct
  {
    float	start;		/* Linear (density) start value for curve */
    float	end;		/* Linear (density) end value for curve */
    float	gamma;		/* Gamma correction */
  }		curve;		/* Curve values */
  struct
  {
    int		num_elements;	/* Number of array elements */
    int		*elements;	/* Array of integer values */
  }		integer_array;	/* Integer array value */
  struct
  {
    int		num_elements;	/* Number of array elements */
    float	*elements;	/* Array of real values */
  }		real_array;	/* Real array value */
  struct
  {
    int		num_elements;	/* Number of array elements */
    float	*elements;	/* Array of XY values */
  }		xy_array;	/* XY array value */
} ppd_ext_value_t;

typedef struct ppd_ext_param_str/**** Extended Parameter ****/
{
  char		keyword[PPD_MAX_NAME];
				/* Parameter name */
  char		text[PPD_MAX_TEXT];
				/* Human-readable text */
  ppd_ext_value_t *value;	/* Current values */
  ppd_ext_value_t *defval;	/* Default values */
  ppd_ext_value_t *minval;	/* Minimum numeric values */
  ppd_ext_value_t *maxval;	/* Maximum numeric values */
} ppd_ext_param_t;

typedef struct ppd_ext_option_str
				/**** Extended Options ****/
{
  char		keyword[PPD_MAX_NAME];
				/* Name of option that is being extended... */
  ppd_option_t	*option;	/* Option that is being extended... */
  int		marked;		/* Extended option is marked */
  char		*code;		/* Generic PS code for extended options */
  int		num_params;	/* Number of parameters */
  ppd_ext_param_t **params;	/* Parameters */
} ppd_ext_option_t;
#  endif /* 0 */

typedef struct ppd_file_str	/**** Files ****/
{
  int		language_level;	/* Language level of device */
  int		color_device;	/* 1 = color device, 0 = grayscale */
  int		variable_sizes;	/* 1 = supports variable sizes, 0 = doesn't */
  int		accurate_screens;
				/* 1 = supports accurate screens, 0 = not */
  int		contone_only;	/* 1 = continuous tone only, 0 = not */
  int		landscape;	/* -90 or 90 */
  int		model_number;	/* Device-specific model number */
  int		manual_copies;	/* 1 = Copies done manually, 0 = hardware */
  int		throughput;	/* Pages per minute */
  ppd_cs_t	colorspace;	/* Default colorspace */
  char		*patches;	/* Patch commands to be sent to printer */
  int		num_emulations;	/* Number of emulations supported */
  ppd_emul_t	*emulations;	/* Emulations and the code to invoke them */
  char		*jcl_begin;	/* Start JCL commands */
  char		*jcl_ps;	/* Enter PostScript interpreter */
  char		*jcl_end;	/* End JCL commands */
  char		*lang_encoding;	/* Language encoding */
  char		*lang_version;	/* Language version (English, Spanish, etc.) */
  char		*modelname;	/* Model name (general) */
  char		*ttrasterizer;	/* Truetype rasterizer */
  char		*manufacturer;	/* Manufacturer name */
  char		*product;	/* Product name (from PS RIP/interpreter) */
  char		*nickname;	/* Nickname (specific) */
  char		*shortnickname;	/* Short version of nickname */
  int		num_groups;	/* Number of UI groups */
  ppd_group_t	*groups;	/* UI groups */
  int		num_sizes;	/* Number of page sizes */
  ppd_size_t	*sizes;		/* Page sizes */
  float		custom_min[2];	/* Minimum variable page size */
  float		custom_max[2];	/* Maximum variable page size */
  float		custom_margins[4];/* Margins around page */
  int		num_consts;	/* Number of UI/Non-UI constraints */
  ppd_const_t	*consts;	/* UI/Non-UI constraints */
  int		num_fonts;	/* Number of pre-loaded fonts */
  char		**fonts;	/* Pre-loaded fonts */
  int		num_profiles;	/* Number of sRGB color profiles */
  ppd_profile_t	*profiles;	/* sRGB color profiles */
  int		num_filters;	/* Number of filters */
  char		**filters;	/* Filter strings... */

  /**** New in CUPS 1.1 ****/
  int		flip_duplex;	/* 1 = Flip page for back sides */

  /**** New in CUPS 1.1.19 ****/
  char		*protocols;	/* Protocols (BCP, TBCP) string */
  char		*pcfilename;	/* PCFileName string */
  int		num_attrs;	/* Number of attributes */
  int		cur_attr;	/* Current attribute */
  ppd_attr_t	**attrs;	/* Attributes */

  /**** New in CUPS 1.2 ****/
#  if 0
  int		num_extended;	/* Number of extended options */
  ppd_ext_option_t **extended;	/* Extended options */
#  endif /* 0 */
} ppd_file_t;


/*
 * Prototypes...
 */

extern void		ppdClose(ppd_file_t *ppd);
extern int		ppdCollect(ppd_file_t *ppd, ppd_section_t section,
			           ppd_choice_t  ***choices);
extern int		ppdConflicts(ppd_file_t *ppd);
extern int		ppdEmit(ppd_file_t *ppd, FILE *fp,
			        ppd_section_t section);
extern int		ppdEmitFd(ppd_file_t *ppd, int fd,
			          ppd_section_t section);
extern int		ppdEmitJCL(ppd_file_t *ppd, FILE *fp, int job_id,
			           const char *user, const char *title);
extern ppd_choice_t	*ppdFindChoice(ppd_option_t *o, const char *option);
extern ppd_choice_t	*ppdFindMarkedChoice(ppd_file_t *ppd, const char *keyword);
extern ppd_option_t	*ppdFindOption(ppd_file_t *ppd, const char *keyword);
extern int		ppdIsMarked(ppd_file_t *ppd, const char *keyword,
			            const char *option);
extern void		ppdMarkDefaults(ppd_file_t *ppd);
extern int		ppdMarkOption(ppd_file_t *ppd, const char *keyword,
			              const char *option);
extern ppd_file_t	*ppdOpen(FILE *fp);
extern ppd_file_t	*ppdOpenFd(int fd);
extern ppd_file_t	*ppdOpenFile(const char *filename);
extern float		ppdPageLength(ppd_file_t *ppd, const char *name);
extern ppd_size_t	*ppdPageSize(ppd_file_t *ppd, const char *name);
extern float		ppdPageWidth(ppd_file_t *ppd, const char *name);

/**** New in CUPS 1.1.19 ****/
extern const char	*ppdErrorString(ppd_status_t status);
extern ppd_attr_t	*ppdFindAttr(ppd_file_t *ppd, const char *name,
			             const char *spec);
extern ppd_attr_t	*ppdFindNextAttr(ppd_file_t *ppd, const char *name,
			                 const char *spec);
extern ppd_status_t	ppdLastError(int *line);

/**** New in CUPS 1.1.20 ****/
extern void		ppdSetConformance(ppd_conform_t c);

/**** New in CUPS 1.2 ****/
extern int		ppdEmitJCLEnd(ppd_file_t *ppd, FILE *fp);
extern ppd_file_t	*ppdOpen2(cups_file_t *fp);


/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_PPD_H_ */

/*
 * End of "$Id$".
 */
