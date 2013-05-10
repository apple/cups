/* Copyright (C) 1990, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gstype1.h */
/* Client interface to Adobe Type 1 font routines for Ghostscript library */

/* ------ Normal client interface ------ */

#define crypt_charstring_seed 4330
typedef struct gs_type1_state_s gs_type1_state;
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif
#ifndef gs_show_enum_s_DEFINED
struct gs_show_enum_s;
#endif
#ifndef gs_type1_data_s_DEFINED
struct gs_type1_data_s;
#endif
int gs_type1_interp_init(P7(gs_type1_state *pcis, gs_imager_state *pis,
			    gx_path *ppath, const gs_log2_scale_point *pscale,
			    bool charpath_flag, int paint_type,
			    gs_font_type1 *pfont));
void gs_type1_set_lsb(P2(gs_type1_state *pcis, const gs_point *psbpt));
void gs_type1_set_width(P2(gs_type1_state *pcis, const gs_point *pwpt));
/* Backward compatibility */
#define gs_type1_init(pcis, penum, psbpt, charpath_flag, paint_type, pfont)\
  (gs_type1_interp_init(pcis, (gs_imager_state *)((penum)->pgs),\
			(penum)->pgs->path, &(penum)->log2_current_scale,\
			charpath_flag, paint_type, pfont) |\
   ((psbpt) == 0 ? 0 : (gs_type1_set_lsb(pcis, psbpt), 0)))
/*
 * Continue interpreting a Type 1 CharString.
 * If str != 0, it is taken as the byte string to interpret.
 * Return 0 on successful completion, <0 on error,
 * or >0 when client intervention is required (or allowed).
 * The int * argument is where the character is stored for seac,
 * or the othersubr # for callothersubr.
 */
#define type1_result_sbw 1		/* allow intervention after [h]sbw */
#define type1_result_callothersubr 2
/*#define type1_result_seac 3*/		/* no longer used */

int gs_type1_interpret(P3(gs_type1_state *, const gs_const_string *, int *));

/* ------ CharString representation ------ */

/* Define the charstring command set */
typedef enum {
		c_undef0 = 0,
	c_hstem = 1,
		c_undef2 = 2,
	c_vstem = 3,
	c_vmoveto = 4,
	c_rlineto = 5,
	c_hlineto = 6,
	c_vlineto = 7,
	c_rrcurveto = 8,
	c_closepath = 9,
	c_callsubr = 10,
	c_return = 11,
	c_escape = 12,			/* extends the command set */
	c_hsbw = 13,
	c_endchar = 14,
	c_undoc15 = 15,			/* An obsolete and undocumented */
					/* 'moveto' command, */
					/* used in some Adobe fonts. */
		c_undef16 = 16,
		c_undef17 = 17,
		c_undef18 = 18,
		c_undef19 = 19,
		c_undef20 = 20,
	c_rmoveto = 21,
	c_hmoveto = 22,
		c_undef23 = 23,
		c_undef24 = 24,
		c_undef25 = 25,
		c_undef26 = 26,
		c_undef27 = 27,
		c_undef28 = 28,
		c_undef29 = 29,
	c_vhcurveto = 30,
	c_hvcurveto = 31,

		/* Values from 32 to 246 represent small integers. */
	c_num1 = 32,
#define c_value_num1(ch) ((int)(byte)(ch) - 139)
		/* We have to declare all these values in the enumeration */
		/* so that some compilers won't complain when we use them */
		/* in the big switch statement. */
#define c_v8(v,a,b,c,d,e,f,g,h)\
  a=v, b=v+1, c=v+2, d=v+3, e=v+4, f=v+5, g=v+6, h=v+7
#define c_v9(v,a,b,c,d,e,f,g,h,i)\
  c_v8(v,a,b,c,d,e,f,g,h), i=v+8
#define c_v10(v,a,b,c,d,e,f,g,h,i,j)\
  c_v9(v,a,b,c,d,e,f,g,h,i), j=v+9
	c_v8( 32, c_n107,c_n106,c_n105,c_n104,c_n103,c_n102,c_n101,c_n100),
	c_v10(40, c_n99,c_n98,c_n97,c_n96,c_n95,c_n94,c_n93,c_n92,c_n91,c_n90),
	c_v10(50, c_n89,c_n88,c_n87,c_n86,c_n85,c_n84,c_n83,c_n82,c_n81,c_n80),
	c_v10(60, c_n79,c_n78,c_n77,c_n76,c_n75,c_n74,c_n73,c_n72,c_n71,c_n70),
	c_v10(70, c_n69,c_n68,c_n67,c_n66,c_n65,c_n64,c_n63,c_n62,c_n61,c_n60),
	c_v10(80, c_n59,c_n58,c_n57,c_n56,c_n55,c_n54,c_n53,c_n52,c_n51,c_n50),
	c_v10(90, c_n49,c_n48,c_n47,c_n46,c_n45,c_n44,c_n43,c_n42,c_n41,c_n40),
	c_v10(100,c_n39,c_n38,c_n37,c_n36,c_n35,c_n34,c_n33,c_n32,c_n31,c_n30),
	c_v10(110,c_n29,c_n28,c_n27,c_n26,c_n25,c_n24,c_n23,c_n22,c_n21,c_n20),
	c_v10(120,c_n19,c_n18,c_n17,c_n16,c_n15,c_n14,c_n13,c_n12,c_n11,c_n10),
	c_v9( 130, c_n9, c_n8, c_n7, c_n6, c_n5, c_n4, c_n3, c_n2, c_n1),
	c_v10(139,  c_0,  c_1,  c_2,  c_3,  c_4,  c_5,  c_6,  c_7,  c_8,  c_9),
	c_v10(149, c_10, c_11, c_12, c_13, c_14, c_15, c_16, c_17, c_18, c_19),
	c_v10(159, c_20, c_21, c_22, c_23, c_24, c_25, c_26, c_27, c_28, c_29),
	c_v10(169, c_30, c_31, c_32, c_33, c_34, c_35, c_36, c_37, c_38, c_39),
	c_v10(179, c_40, c_41, c_42, c_43, c_44, c_45, c_46, c_47, c_48, c_49),
	c_v10(189, c_50, c_51, c_52, c_53, c_54, c_55, c_56, c_57, c_58, c_59),
	c_v10(199, c_60, c_61, c_62, c_63, c_64, c_65, c_66, c_67, c_68, c_69),
	c_v10(209, c_70, c_71, c_72, c_73, c_74, c_75, c_76, c_77, c_78, c_79),
	c_v10(219, c_80, c_81, c_82, c_83, c_84, c_85, c_86, c_87, c_88, c_89),
	c_v10(229, c_90, c_91, c_92, c_93, c_94, c_95, c_96, c_97, c_98, c_99),
	c_v8( 239,c_100,c_101,c_102,c_103,c_104,c_105,c_106,c_107),

		/* The next 4 values represent 2-byte positive integers. */
	c_pos2_0 = 247,
	c_pos2_1 = 248,
	c_pos2_2 = 249,
	c_pos2_3 = 250,
#define c_value_pos2(c1,c2)\
  (((int)(byte)((c1) - (int)c_pos2_0) << 8) + (int)(byte)(c2) + 108)

		/* The next 4 values represent 2-byte negative integers. */
	c_neg2_0 = 251,
	c_neg2_1 = 252,
	c_neg2_2 = 253,
	c_neg2_3 = 254,
#define c_value_neg2(c1,c2)\
  -(((int)(byte)((c1) - (int)c_neg2_0) << 8) + (int)(byte)(c2) + 108)

		/* Finally, there is an escape for 4-byte 2's complement */
		/* numbers in big-endian order. */
	c_num4 = 255
} char_command;
typedef enum {				/* extended commands */
	ce_dotsection = 0,
	ce_vstem3 = 1,
	ce_hstem3 = 2,
	ce_seac = 6,
	ce_sbw = 7,
	ce_div = 12,
	ce_undoc15 = 15,		/* An obsolete and undocumented */
					/* 'addifgt' command, */
					/* used in some Adobe fonts. */
	ce_callothersubr = 16,
	ce_pop = 17,
	ce_setcurrentpoint = 33
} char_extended_command;
