/*
 * "$Id: ppd.h,v 1.14 1999/04/21 14:12:21 mike Exp $"
 *
 *   PostScript Printer Description definitions for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-1999 by Easy Software Products, all rights reserved.
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
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
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
 */

#ifndef _CUPS_PPD_H_
#  define _CUPS_PPD_H_

/*
 * Include necessary headers...
 */

#  include <stdio.h>


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
 * Types and structures...
 */

typedef enum			/**** UI types ****/
{
  PPD_UI_BOOLEAN,		/* True or False option */
  PPD_UI_PICKONE,		/* Pick one from a list */
  PPD_UI_PICKMANY		/* Pick zero or more from a list */
} ppd_ui_t;

typedef enum			/**** Order dependency sections ****/
{
  PPD_ORDER_ANY,		/* Option code can be anywhere in the file */
  PPD_ORDER_DOCUMENT,		/* ... must be in the DocumentSetup section */
  PPD_ORDER_EXIT,		/* ... must be sent prior to the document */
  PPD_ORDER_JCL,		/* ... must be sent as a JCL command */
  PPD_ORDER_PAGE,		/* ... must be in the PageSetup section */
  PPD_ORDER_PROLOG		/* ... must be in the Prolog section */
} ppd_section_t;

typedef enum			/**** Colorspaces ****/
{
  PPD_CS_CMYK = -4,		/* CMYK colorspace */
  PPD_CS_CMY,			/* CMY colorspace */
  PPD_CS_GRAY = 1,		/* Grayscale colorspace */
  PPD_CS_RGB = 3,		/* RGB colorspace */
  PPD_CS_RGBK,			/* RGBK (K = gray) colorspace */
  PPD_CS_N			/* DeviceN colorspace */
} ppd_cs_t;

typedef struct			/**** Option choices ****/
{
  char		marked,		/* 0 if not selected, 1 otherwise */
		choice[41],	/* Computer-readable option name */
		text[81],	/* Human-readable option name */
		*code;		/* Code to send for this option */
  void		*option;	/* Pointer to parent option structure */
} ppd_choice_t;

typedef struct			/**** Options ****/
{
  char		conflicted,	/* 0 if no conflicts exist, 1 otherwise */
		keyword[41],	/* Option keyword name ("PageSize", etc.) */
		defchoice[41],	/* Default option choice */
		text[81];	/* Human-readable text */
  ppd_ui_t	ui;		/* Type of UI option */
  ppd_section_t	section;	/* Section for command */
  float		order;		/* Order number */
  int		num_choices;	/* Number of option choices */
  ppd_choice_t	*choices;	/* Option choices */
} ppd_option_t;

typedef struct ppd_group_str	/**** Groups ****/
{
  char		text[81];	/* Human-readable group name */
  int		num_options;	/* Number of options */
  ppd_option_t	*options;	/* Options */
  int		num_subgroups;	/* Number of sub-groups */
  struct ppd_group_str	*subgroups;
				/* Sub-groups (max depth = 1) */
} ppd_group_t;

typedef struct			/**** Constraints ****/
{
  char		option1[41],	/* First keyword */
		choice1[41],	/* First option/choice (blank for all) */
		option2[41],	/* Second keyword */
		choice2[41];	/* Second option/choice (blank for all) */
} ppd_const_t;

typedef struct			/**** Page Sizes ****/
{
  int		marked;		/* Page size selected? */
  char		name[41];	/* Media size option */
  float		width,		/* Width of media in points */
		length,		/* Length of media in points */
		left,		/* Left printable margin in points */
		bottom,		/* Bottom printable margin in points */
		right,		/* Right printable margin in points */
		top;		/* Top printable margin in points */
} ppd_size_t;

typedef struct			/**** Emulators ****/
{
  char		name[41],	/* Emulator name */
		*start,		/* Code to switch to this emulation */
		*stop;		/* Code to stop this emulation */
} ppd_emul_t;

typedef struct			/**** sRGB Color Profiles ****/
{
  char		resolution[41],	/* Resolution or "-" */
		media_type[41];	/* Media type of "-" */
  float		density,	/* Ink density to use */
		matrix[3][3];	/* Transform matrix */
} ppd_profile_t;

typedef struct			/**** Files ****/
{
  int		language_level,	/* Language level of device */
		color_device,	/* 1 = color device, 0 = grayscale */
		variable_sizes,	/* 1 = supports variable sizes, 0 = doesn't */
		accurate_screens,/* 1 = supports accurate screens, 0 = not */
		contone_only,	/* 1 = continuous tone only, 0 = not */
		landscape,	/* -90 or 90 */
		model_number,	/* Device-specific model number */
		manual_copies;	/* 1 = Copies done manually, 0 = hardware */
  ppd_cs_t	colorspace;	/* Default colorspace */
  char		*patches;	/* Patch commands to be sent to printer */
  int		num_emulations;	/* Number of emulations supported */
  ppd_emul_t	*emulations;	/* Emulations and the code to invoke them */
  char		*jcl_begin,	/* Start JCL commands */
		*jcl_ps,	/* Enter PostScript interpreter */
		*jcl_end,	/* End JCL commands */
		*lang_encoding,	/* Language encoding */
		*lang_version,	/* Language version (English, Spanish, etc.) */
		*modelname,	/* Model name (general) */
		*ttrasterizer,	/* Truetype rasterizer */
		*manufacturer,	/* Manufacturer name */
		*product,	/* Product name (from PS RIP/interpreter) */
		*nickname,	/* Nickname (specific) */
		*shortnickname;	/* Short version of nickname */
  int		num_groups;	/* Number of UI groups */
  ppd_group_t	*groups;	/* UI groups */
  int		num_sizes;	/* Number of page sizes */
  ppd_size_t	*sizes;		/* Page sizes */
  float		custom_min[2],	/* Minimum variable page size */
		custom_max[2],	/* Maximum variable page size */
		custom_margins[4];/* Margins around page */
  int		num_consts;	/* Number of UI/Non-UI constraints */
  ppd_const_t	*consts;	/* UI/Non-UI constraints */
  int		num_fonts;	/* Number of pre-loaded fonts */
  char		**fonts;	/* Pre-loaded fonts */
  int		num_profiles;	/* Number of sRGB color profiles */
  ppd_profile_t	*profiles;	/* sRGB color profiles */
} ppd_file_t;


/*
 * Prototypes...
 */

extern void		ppdClose(ppd_file_t *ppd);
extern int		ppdConflicts(ppd_file_t *ppd);
extern int		ppdEmit(ppd_file_t *ppd, FILE *fp,
			        ppd_section_t section);
extern int		ppdEmitFd(ppd_file_t *ppd, int fd,
			          ppd_section_t section);
extern int		ppdIsMarked(ppd_file_t *ppd, char *keyword,
			            char *option);
extern void		ppdMarkDefaults(ppd_file_t *ppd);
extern int		ppdMarkOption(ppd_file_t *ppd, char *keyword,
			              char *option);
extern ppd_choice_t	*ppdFindChoice(ppd_option_t *o, char *option);
extern ppd_choice_t	*ppdFindMarkedChoice(ppd_file_t *ppd, char *keyword);
extern ppd_option_t	*ppdFindOption(ppd_file_t *ppd, char *keyword);
extern ppd_file_t	*ppdOpen(FILE *fp);
extern ppd_file_t	*ppdOpenFd(int fd);
extern ppd_file_t	*ppdOpenFile(char *filename);
extern float		ppdPageLength(ppd_file_t *ppd, char *name);
extern ppd_size_t	*ppdPageSize(ppd_file_t *ppd, char *name);
extern float		ppdPageWidth(ppd_file_t *ppd, char *name);

/*
 * C++ magic...
 */

#  ifdef __cplusplus
}
#  endif /* __cplusplus */
#endif /* !_CUPS_PPD_H_ */

/*
 * End of "$Id: ppd.h,v 1.14 1999/04/21 14:12:21 mike Exp $".
 */
