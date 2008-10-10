/*
 * "$Id: hpgltops.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   HP-GL/2 to PostScript filter for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "common.h"
#include <math.h>

#ifndef M_PI
#  define M_PI	3.14159265358979323846
#endif /* M_PI */


/*
 * Maximum number of pens we emulate...
 */

#define MAX_PENS	1024


/*
 * Parameter value structure...
 */

typedef struct
{
  int	type;
  union
  {
    float	number;
    char	*string;
  }	value;
} param_t;

#define PARAM_ABSOLUTE	0
#define PARAM_RELATIVE	1
#define PARAM_STRING	2


/*
 * Font information...
 */

typedef struct
{
  int	symbol_set,	/* Symbol set */
	spacing,	/* Spacing (0 = fixed, 1 = proportional) */
	posture,	/* Posture number */
	weight,		/* Weight number */
	typeface;	/* Typeface number */
  float	pitch,		/* Characters per inch */
	height,		/* Height/size of font */
	xpitch;		/* X pitch scaling value */
  float	x, y;		/* X and Y direction/scaling */
} font_t;


/*
 * Pen information...
 */

typedef struct
{
  float	rgb[3];		/* Pen color */
  float	width;		/* Pen width */
} pen_t;


/*
 * Globals...
 */

#ifdef _HPGL_MAIN_C_
#  define VAR
#  define VALUE(x) =x
#  define VALUE2(x,y) ={x,y}
#else
#  define VAR extern
#  define VALUE(x)
#  define VALUE2(x,y)
#endif /* _HPGL_MAIN_C_ */

VAR ppd_file_t	*PPD VALUE(NULL);		/* PPD file */

VAR float	P1[2],				/* Lower-lefthand physical limit */
		P2[2],				/* Upper-righthand physical limit */
		IW1[2],				/* Window lower-lefthand limit */
		IW2[2];				/* Window upper-righthand limit */
VAR int		Rotation	VALUE(0);	/* Page rotation */
VAR int		ScalingType	VALUE(-1);	/* Type of scaling (-1 for none) */
VAR float	Scaling1[2],			/* Lower-lefthand user limit */
		Scaling2[2];			/* Upper-righthand user limit */
VAR float	Transform[2][3];		/* Transform matrix */
VAR int		PageRotation	VALUE(0);	/* Page/plot rotation */

VAR char	StringTerminator VALUE('\003');	/* Terminator for labels */
VAR font_t	StandardFont,			/* Standard font */
		AlternateFont;			/* Alternate font */
VAR float	PenPosition[2]	VALUE2(0.0f, 0.0f),
						/* Current pen position */
		PenScaling	VALUE(1.0f),	/* Pen width scaling factor */
		PenWidth	VALUE(1.0f);	/* Default pen width */
VAR pen_t	Pens[MAX_PENS];			/* State of each pen */
VAR int		PenMotion	VALUE(0), 	/* 0 = absolute, 1 = relative */
		PenValid	VALUE(0),	/* 1 = valid position, 0 = undefined */
		PenNumber	VALUE(0),	/* Current pen number */
		PenCount	VALUE(8),	/* Number of pens */
		PenDown		VALUE(0),	/* 0 = pen up, 1 = pen down */
		PolygonMode	VALUE(0),	/* Drawing polygons? */
		PageCount	VALUE(0),	/* Number of pages in plot */
		PageDirty	VALUE(0),	/* Current page written on? */
		WidthUnits	VALUE(0);	/* 0 = mm, 1 = proportionate */
VAR float	PlotSize[2]	VALUE2(2592.0f, 3456.0f);
						/* Plot size */
VAR int		PlotSizeSet	VALUE(0);	/* Plot size set? */
VAR int		CharFillMode	VALUE(0),	/* Where to draw labels */
		CharPen		VALUE(0),	/* Pen to use for labels */
		CharFont	VALUE(0);	/* Font to use for labels */
VAR float	CharHeight[2]	VALUE2(11.5f,11.5f);
						/* Size of font for labels */
VAR int		FitPlot		VALUE(0);	/* 1 = fit to page */
VAR float	ColorRange[3][2]		/* Range of color values */
#ifdef _HPGL_MAIN_C_
		= {
		  { 0.0, 255.0 },
		  { 0.0, 255.0 },
		  { 0.0, 255.0 }
		}
#endif /* _HPGL_MAIN_C_ */
;

VAR int		LineCap		VALUE(0);	/* Line capping */
VAR int		LineJoin	VALUE(0);	/* Line joining */
VAR float	MiterLimit	VALUE(3.0f);	/* Miter limit at joints */


/*
 * Prototypes...
 */

/* hpgl-input.c */
extern int	ParseCommand(FILE *fp, char *name, param_t **params);
extern void	FreeParameters(int num_params, param_t *params);

/* hpgl-config.c */
extern void	update_transform(void);
extern void	BP_begin_plot(int num_params, param_t *params);
extern void	DF_default_values(int num_params, param_t *params);
extern void	IN_initialize(int num_params, param_t *params);
extern void	IP_input_absolute(int num_params, param_t *params);
extern void	IR_input_relative(int num_params, param_t *params);
extern void	IW_input_window(int num_params, param_t *params);
extern void	PG_advance_page(int num_params, param_t *params);
extern void	PS_plot_size(int num_params, param_t *params);
extern void	RO_rotate(int num_params, param_t *params);
extern void	RP_replot(int num_params, param_t *params);
extern void	SC_scale(int num_params, param_t *params);

/* hpgl-vector.c */
extern void	AA_arc_absolute(int num_params, param_t *params);
extern void	AR_arc_relative(int num_params, param_t *params);
extern void	AT_arc_absolute3(int num_params, param_t *params);
extern void	CI_circle(int num_params, param_t *params);
extern void	PA_plot_absolute(int num_params, param_t *params);
extern void	PD_pen_down(int num_params, param_t *params);
extern void	PE_polyline_encoded(int num_params, param_t *params);
extern void	PR_plot_relative(int num_params, param_t *params);
extern void	PU_pen_up(int num_params, param_t *params);
extern void	RT_arc_relative3(int num_params, param_t *params);

/* hpgl-polygon.c */
extern void	EA_edge_rect_absolute(int num_params, param_t *params);
extern void	EP_edge_polygon(int num_params, param_t *params);
extern void	ER_edge_rect_relative(int num_params, param_t *params);
extern void	EW_edge_wedge(int num_params, param_t *params);
extern void	FP_fill_polygon(int num_params, param_t *params);
extern void	PM_polygon_mode(int num_params, param_t *params);
extern void	RA_fill_rect_absolute(int num_params, param_t *params);
extern void	RR_fill_rect_relative(int num_params, param_t *params);
extern void	WG_fill_wedge(int num_params, param_t *params);

/* hpgl-char.c */
extern void	define_font(int f);
extern void	AD_define_alternate(int num_params, param_t *params);
extern void	CF_character_fill(int num_params, param_t *params);
extern void	CP_character_plot(int num_params, param_t *params);
extern void	DI_absolute_direction(int num_params, param_t *params);
extern void	DR_relative_direction(int num_params, param_t *params);
extern void	DT_define_label_term(int num_params, param_t *params);
extern void	DV_define_variable_path(int num_params, param_t *params);
extern void	ES_extra_space(int num_params, param_t *params);
extern void	LB_label(int num_params, param_t *params);
extern void	LO_label_origin(int num_params, param_t *params);
extern void	SA_select_alternate(int num_params, param_t *params);
extern void	SD_define_standard(int num_params, param_t *params);
extern void	SI_absolute_size(int num_params, param_t *params);
extern void	SL_character_slant(int num_params, param_t *params);
extern void	SR_relative_size(int num_params, param_t *params);
extern void	SS_select_standard(int num_params, param_t *params);
extern void	TD_transparent_data(int num_params, param_t *params);

/* hpgl-attr.c */
extern void	AC_anchor_corner(int num_params, param_t *params);
extern void	CR_color_range(int num_params, param_t *params);
extern void	FT_fill_type(int num_params, param_t *params);
extern void	LA_line_attributes(int num_params, param_t *params);
extern void	LT_line_type(int num_params, param_t *params);
extern void	NP_number_pens(int num_params, param_t *params);
extern void	PC_pen_color(int num_params, param_t *params);
extern void	PW_pen_width(int num_params, param_t *params);
extern void	RF_raster_fill(int num_params, param_t *params);
extern void	SM_symbol_mode(int num_params, param_t *params);
extern void	SP_select_pen(int num_params, param_t *params);
extern void	UL_user_line_type(int num_params, param_t *params);
extern void	WU_width_units(int num_params, param_t *params);

/* hpgl-prolog.c */
extern void	OutputProlog(char *title, char *user, int shading);
extern void	OutputTrailer(void);
extern int	Outputf(const char *format, ...);

/*
 * End of "$Id: hpgltops.h 6649 2007-07-11 21:46:42Z mike $".
 */
