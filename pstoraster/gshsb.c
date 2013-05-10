/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* gshsb.c */
/* HSB color operators for Ghostscript library */
#include "gx.h"
#include "gscolor.h"
#include "gshsb.h"		/* interface definition */
#include "gxfrac.h"

/* Forward references */
private void color_hsb_to_rgb(P4(floatp h, floatp s, floatp b, float rgb[3]));
private void color_rgb_to_hsb(P4(floatp r, floatp g, floatp b, float hsb[3]));

/* Force a parameter into the range [0.0..1.0]. */
#define force_unit(p) (p < 0.0 ? 0.0 : p > 1.0 ? 1.0 : p)

/* sethsbcolor */
int
gs_sethsbcolor(gs_state *pgs, floatp h, floatp s, floatp b)
{	float rgb[3];
	color_hsb_to_rgb(force_unit(h), force_unit(s), force_unit(b), rgb);
	return gs_setrgbcolor(pgs, rgb[0], rgb[1], rgb[2]);
}

/* currenthsbcolor */
int
gs_currenthsbcolor(const gs_state *pgs, float pr3[3])
{	float rgb[3];
	gs_currentrgbcolor(pgs, rgb);
	color_rgb_to_hsb(rgb[0], rgb[1], rgb[2], pr3);
	return 0;
}

/* ------ Internal routines ------ */

/* Note: the color model conversion algorithms are taken from */
/* Rogers, Procedural Elements for Computer Graphics, pp. 401-403. */

/* Convert RGB to HSB. */
private void
color_rgb_to_hsb(floatp r, floatp g, floatp b, float hsb[3])
{	frac red = float2frac(r), green = float2frac(g), blue = float2frac(b);
#define rhue hsb[0]
#define rsat hsb[1]
#define rbri hsb[2]
	if ( red == green && green == blue )
	   {	rhue = 0;	/* arbitrary */
		rsat = 0;
		rbri = r;	/* pick any one */
	   }
	else
	   {	/* Convert rgb to hsb */
		frac V, Temp;
		long diff, H;
		V = (red > green ? red : green);
		if ( blue > V ) V = blue;
		Temp = (red > green ? green : red);
		if ( blue < Temp ) Temp = blue;
		diff = V - Temp;
		if ( V == red )
		    H = (green - blue) * frac_1_long / diff;
		else if ( V == green )
		    H = (blue - red) * frac_1_long / diff + 2 * frac_1_long;
		else /* V == blue */
		    H = (red - green) * frac_1_long / diff + 4 * frac_1_long;
		if ( H < 0 ) H += 6 * frac_1_long;
		rhue = H / (frac_1 * 6.0);
		rsat = diff / (float)V;
		rbri = frac2float(V);
	   }
#undef rhue
#undef rsat
#undef rbri
}

/* Convert HSB to RGB. */
private void
color_hsb_to_rgb(floatp hue, floatp saturation, floatp brightness, float rgb[3])
{	if ( saturation == 0 )
	   {	rgb[0] = rgb[1] = rgb[2] = brightness;
	   }
	else
	   {	/* Convert hsb to rgb. */
		/* We rely on the fact that the product of two */
		/* fracs fits into an unsigned long. */
		floatp h6 = hue * 6;
		ulong V = float2frac(brightness);	/* force arithmetic to long */
		frac S = float2frac(saturation);
		int I = (int)h6;
		ulong F = float2frac(h6 - I);		/* ditto */
		/* M = V*(1-S), N = V*(1-S*F), K = V*(1-S*(1-F)) = M-N+V */
		frac M = V * (frac_1_long - S) / frac_1_long;
		frac N = V * (frac_1_long - S * F / frac_1_long) / frac_1_long;
		frac K = M - N + V;
		frac R, G, B;
		switch ( I )
		   {
		default: R = V; G = K; B = M; break;
		case 1: R = N; G = V; B = M; break;
		case 2: R = M; G = V; B = K; break;
		case 3: R = M; G = N; B = V; break;
		case 4: R = K; G = M; B = V; break;
		case 5: R = V; G = M; B = N; break;
		   }
		rgb[0] = frac2float(R);
		rgb[1] = frac2float(G);
		rgb[2] = frac2float(B);
#ifdef DEBUG
if ( gs_debug_c('c') )
{		dprintf7("[c]hsb(%g,%g,%g)->VSFI(%ld,%d,%ld,%d)->\n",
			 hue, saturation, brightness, V, S, F, I);
		dprintf6("   RGB(%d,%d,%d)->rgb(%g,%g,%g)\n",
			 R, G, B, rgb[0], rgb[1], rgb[2]);
}
#endif
	   }
}
