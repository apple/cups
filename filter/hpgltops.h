/*
 * "$Id: hpgltops.h,v 1.1 1996/08/24 19:41:24 mike Exp $"
 *
 *   HPGL to PostScript conversion program header file for espPrint, a
 *   collection of printer/image software.
 *
 *   Copyright 1993-1996 by Easy Software Products
 *
 *   These coded instructions, statements, and computer  programs  contain
 *   unpublished  proprietary  information  of Easy Software Products, and
 *   are protected by Federal copyright law.  They may  not  be  disclosed
 *   to  third  parties  or  copied or duplicated in any form, in whole or
 *   in part, without the prior written consent of Easy Software Products.
 *
 * Revision History:
 *
 *   $Log: hpgltops.h,v $
 *   Revision 1.1  1996/08/24 19:41:24  mike
 *   Initial revision
 *
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <pod.h>


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
 * Globals...
 */

#ifdef _MAIN_C_
#  define VAR
#  define VALUE(x) =x
#  define VALUE2(x,y) ={x,y}
#else
#  define VAR extern
#  define VALUE(x)
#  define VALUE2(x,y)
#endif /* _MAIN_C_ */

VAR FILE	*InputFile,	/* Input file */
		*OutputFile;	/* Output file */

VAR int		P1[2],		/* Lower-lefthand physical limit */
		P2[2];		/* Upper-righthand physical limit */
VAR int		Rotation VALUE(0);	/* Page rotation */
VAR int		ScalingType VALUE(-1);	/* Type of scaling (-1 for none) */
VAR float	Scaling1[2],	/* Lower-lefthand user limit */
		Scaling2[2];	/* Upper-righthand user limit */
VAR float	Transform[3][2];
VAR float	PageWidth, PageHeight, PageLeft, PageBottom;

VAR char	StringTerminator VALUE('\003');
VAR float	PenPosition[2] VALUE({0}),
		PenWidth VALUE(1.0),
		PenMotion VALUE(0); /* 0 = absolute, 1 = relative */
VAR int		PenDown VALUE(0),
		PolygonMode VALUE(0),
		PageCount VALUE(1);

VAR int		CharFillMode VALUE(0),
		CharPen VALUE(0),
		CharFont VALUE(0);
VAR float	CharHeight[2] VALUE2(11.5,11.5);
VAR int		Verbosity VALUE(0);


/*
 * Prototypes...
 */

extern void	update_transform(void);
extern int	ParseCommand(char *name, param_t **params);
extern void	FreeParameters(int num_params, param_t *params);

extern void	IN_initialize(int num_params, param_t *params);
extern void	DF_default_values(int num_params, param_t *params);
extern void	IP_input_absolute(int num_params, param_t *params);
extern void	IR_input_relative(int num_params, param_t *params);
extern void	IW_input_window(int num_params, param_t *params);
extern void	PG_advance_page(int num_params, param_t *params);
extern void	RO_rotate(int num_params, param_t *params);
extern void	RP_replot(int num_params, param_t *params);
extern void	SC_scale(int num_params, param_t *params);

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

extern void	EA_edge_rect_absolute(int num_params, param_t *params);
extern void	EP_edge_polygon(int num_params, param_t *params);
extern void	ER_edge_rect_relative(int num_params, param_t *params);
extern void	EW_edge_wedge(int num_params, param_t *params);
extern void	FP_fill_polygon(int num_params, param_t *params);
extern void	PM_polygon_mode(int num_params, param_t *params);
extern void	RA_fill_rect_absolute(int num_params, param_t *params);
extern void	RR_fill_rect_relative(int num_params, param_t *params);
extern void	WG_fill_wedge(int num_params, param_t *params);

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

extern void	AC_anchor_corner(int num_params, param_t *params);
extern void	FT_fill_type(int num_params, param_t *params);
extern void	LA_line_attributes(int num_params, param_t *params);
extern void	LT_line_type(int num_params, param_t *params);
extern void	PW_pen_width(int num_params, param_t *params);
extern void	RF_raster_fill(int num_params, param_t *params);
extern void	SM_symbol_mode(int num_params, param_t *params);
extern void	SP_select_pen(int num_params, param_t *params);
extern void	UL_user_line_type(int num_params, param_t *params);
extern void	WU_width_units(int num_params, param_t *params);

extern int	OutputProlog(PDInfoStruct *info);
extern int	OutputTrailer(PDInfoStruct *info);

/*
 * End of "$Id: hpgltops.h,v 1.1 1996/08/24 19:41:24 mike Exp $".
 */
