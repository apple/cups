/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevstc4.c */
/* Epson Stylus-Color Printer-Driver */

/***
     This file holds a byte-Implementation of the Floyd-Steinberg error
     diffusion-algorithm. This algorithm is an alternative for high quality 
     printing in conjunction with the PostScript-Header stcolor.ps:

          gs -sDEVICE=stcolor -sDithering=fs2 <other options> stcolor.ps ...

     THIS ALGORIHM WAS WRITTEN BY STEVEN SINGER (S.Singer@ph.surrey.ac.uk)
     AS PART OF escp2cfs2. 
     THIS IMPLEMENTATION INCORPORATES ONLY FEW CHANGES TO THE ORIGINAL CODE.

 ***/

#include "gdevstc.h"

/*
 * escp2c_pick best scans for best matching color 
 */
private byte *
escp2c_pick_best(byte *col)
{
    static byte colour[8][3] = {
       {  0,  0,  0},{255,  0,  0},{  0,255,  0},{255,255,  0},
       {  0,  0,255},{255,  0,255},{  0,255,255},{255,255,255}};
    register int x, y, z, dx, dy, dz, dz2, dx2, dx3, dx4;
    register byte *p;
    register long md, d;

    md = 16777216; /* plenty */

/*
   Note that I don't use a simple distance algorithm. That can lead to a block
   of (130,127,127) being dithered as red-cyan. This algorithm should make
   it use black-white-red. This is very important, as a coloured block in
   the middle of a grey block can, via error diffusion, perturb the
   surrounding colours sufficiently for this to happen.
*/

/*
   The next bit is equivalent to this, but faster.

    x = col[0];
    y = col[1];
    z = col[2];
    for(n=8; n--; )
    {
	dx = x - colour[n][0];
	dy = y - colour[n][1];
	dz = z - colour[n][2];
	d = dx*(dx-(dy>>1)) + dy*(dy-(dz>>1)) + dz*(dz-(dx>>1));
	if (d < md)
	{
	    md = d;
	    p = n;
	}
    }
*/

/*
 * Test colours in gray code order to reduce number of recalculations.
 * I bet you can't find an optimiser that would do this automatically.
 */

    x = col[0];
    y = col[1];
    z = col[2];
    dx = x*(x-(y>>1));
    dy = y*(y-(z>>1));
    dz = z*(z-(x>>1));
    md = dx + dy + dz;
    p = colour[0];
    x -= 255;
    dx2 = x*(x-(y>>1));
    dz2 = z*(z-(x>>1));
    if ((d = dx2 + dy + dz2) < md) {md = d; p = colour[1];}
    y -= 255;
    dx3 = x*(x-(y>>1));
    dy = y*(y-(z>>1));
    if ((d = dx3 + dy + dz2) < md) {md = d; p = colour[3];}
    x += 255;
    dx4 = x*(x-(y>>1));
    if ((d = dx4 + dy + dz) < md) {md = d; p = colour[2];}
    z -= 255;
    dy = y*(y-(z>>1));
    dz = z*(z-(x>>1));
    if ((d = dx4 + dy + dz) < md) {md = d; p = colour[6];}
    x -= 255;
    dz2 = z*(z-(x>>1));
    if ((d = dx3 + dy + dz2) < md) {md = d; p = colour[7];}
    y += 255;
    dy = y*(y-(z>>1));
    if ((d = dx2 + dy + dz2) < md) {md = d; p = colour[5];}
    if ((d = dx + dy + dz) < md) {p = colour[4];}
    return(p);
}

/*
 * escp2c_conv_stc converts into the ouput format used by stcolor
 */
private void
escp2c_conv_stc(byte *p, byte *q, int i)
{
    for(; i; p+=3, i-=3)
        *q++ = (*p & RED) | (p[1] & GREEN) | (p[2] & BLUE);
}


/*
 * Limit byte-values
 */
#define LIMIT(a) if (a > 255) a = 255; if (a < 0) a = 0
#define LIMIT2(a) if (a > 127) a = 127; if (a < -128) a = -128; \
                if (a < 0) a += 256
/*
 * Main routine of the algorithm
 */
int 
stc_fs2(stcolor_device *sd,int npixel,byte *in,byte *buf,byte *out)
{
   int fullcolor_line_size = npixel*3;

/* ============================================================= */
   if(npixel > 0) {  /* npixel >  0 -> scanline-processing       */
/* ============================================================= */

/*    -------------------------------------------------------------------- */
      if(in == NULL) { /* clear the error-buffer upon white-lines */
/*    -------------------------------------------------------------------- */

         memset(buf,0,fullcolor_line_size);

/*    ------------------------------------------------------------------- */
      } else {                 /* do the actual dithering                 */
/*    ------------------------------------------------------------------- */
    int i, j, k, e, l, i2, below[3][3], *fb, *b, *bb, *tb;
    byte *p, *q, *cp;
    static int dir = 1;

    p = buf;
    if (*p != 0 || memcmp((char *) p, (char *) p + 1, fullcolor_line_size - 1))
    {
	for(p = in, q=buf, i=fullcolor_line_size;
	    i--; p++, q++ )
	{
	    j = *p + ((*q & 128) ? *q - 256 : *q);
	    LIMIT(j);
	    *p = j;
	}
    }

    p = in;

	fb = below[2];
	b = below[1];
	bb = below[0];
	*b = b[1] = b[2] = *bb = bb[1] = bb[2] = 0;

	if (dir)
	{
	    for(p = in, q=buf-3,
		i=fullcolor_line_size; i; i-=3)
	    {
		cp = escp2c_pick_best(p);
		for(i2=3; i2--; p++, q++, fb++, b++, bb++)
		{
		    j = *p;
		    *p = *cp++;
		    j -= *p;
		    if (j != 0)
		    {
			l = (e = (j>>1)) - (*fb = (j>>4));
			if (i > 2)
			{
			    k = p[3] + l;
			    LIMIT(k);
			    p[3] = k;
			}
			*b += e - (l = (j>>2) - *fb);
			if (i < fullcolor_line_size)
			{
			    l += *bb;
			    LIMIT2(l);
			    *q = l;
			}
		    }
		    else
			*fb = 0;
		}
		tb = bb-3;
		bb = b-3;
		b = fb-3;
		fb = tb;
	    }
	    *q = *bb;
	    q[1] = bb[1];
	    q[2] = bb[2];
	    dir = 0;
	}
	else
	{
	    for(p = in+fullcolor_line_size-1,
		q = buf+fullcolor_line_size+2, i=fullcolor_line_size; 
                i; i-=3)
	    {
		cp = escp2c_pick_best(p-2) + 2;
		for(i2=3; i2--; p--, q--, fb++, b++, bb++)
		{
		    j = *p;
		    *p = *cp--;
		    j -= *p;
		    if (j != 0)
		    {
			l = (e = (j>>1)) - (*fb = (j>>4));
			if (i > 2)
			{
			    k = p[-3] + l;
			    LIMIT(k);
			    p[-3] = k;
			}
			*b += e - (l = (j>>2) - *fb);
			if (i < fullcolor_line_size)
			{
			    l += *bb;
			    LIMIT2(l);
			    *q = l;
			}
		    }
		    else
			*fb = 0;
		}
		tb = bb-3;
		bb = b-3;
		b = fb-3;
		fb = tb;
	    }
	    *q = *bb;
	    q[1] = bb[1];
	    q[2] = bb[2];
	    dir = 1;
	}
	
    escp2c_conv_stc(in, out, fullcolor_line_size);

/*    ------------------------------------------------------------------- */
      }                        /* buffer-reset | dithering                */
/*    ------------------------------------------------------------------- */


/* ============================================================= */
   } else {          /* npixel <= 0 -> initialisation            */
/* ============================================================= */


/*
 * check wether the number of components is valid
 */
      if(sd->color_info.num_components != 3)                       return -1;

/*
 * check wether stcdither & TYPE are correct
 */
      if(( sd->stc.dither                    == NULL) ||
         ((sd->stc.dither->flags & STC_TYPE) != STC_BYTE))         return -2;

/*
 * check wether the buffer-size is sufficiently large
 */
      if((sd->stc.dither->flags/STC_SCAN) < 1)                     return -3;

/*
 * finally clear the buffer
 */
      memset(buf,0,-fullcolor_line_size);

/* ============================================================= */
   } /* scanline-processing or initialisation */
/* ============================================================= */

   return 0;
}
