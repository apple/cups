/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gsflip.c */
/* Routines for "flipping" image data */
#include "gx.h"
#include "gsflip.h"

#define arch_has_byte_regs 1

/* Transpose a block of bits between registers. */
#define transpose(r,s,mask,shift)\
  r ^= (temp = ((s << shift) ^ r) & mask);\
  s ^= temp >> shift

/* Define the size of byte temporaries.  On Intel CPUs, this should be */
/* byte, but on all other CPUs, it should be uint. */
#if arch_has_byte_regs
typedef byte byte_var;
#else
typedef uint byte_var;
#endif

#define vtab1(v0,v2,v1)\
  v0,v1+v0,v2+v0,v2+v1+v0
#define vtab2(v0,v10,v8,v4,v2,v1)\
  vtab1(v0,v2,v1), vtab1(v4+v0,v2,v1),\
  vtab1(v8+v0,v2,v1), vtab1(v8+v4+v0,v2,v1),\
  vtab1(v10+v0,v2,v1), vtab1(v10+v4+v0,v2,v1),\
  vtab1(v10+v8+v0,v2,v1), vtab1(v10+v8+v4+v0,v2,v1)
#define vtab(v80,v40,v20,v10,v8,v4,v2,v1)\
  vtab2(0,v10,v8,v4,v2,v1), vtab2(v20,v10,v8,v4,v2,v1),\
  vtab2(v40,v10,v8,v4,v2,v1), vtab2(v40+v20,v10,v8,v4,v2,v1),\
  vtab2(v80,v10,v8,v4,v2,v1), vtab2(v80+v20,v10,v8,v4,v2,v1),\
  vtab2(v80+v40,v10,v8,v4,v2,v1), vtab2(v80+v40+v20,v10,v8,v4,v2,v1)

/* Convert 3Mx1 to 3x1. */
void
flip3x1(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	uint n = nbytes;
	static const far_data bits32 tab3x1[256] = {
		vtab(0x800000,0x100000,0x20000,0x4000,0x800,0x100,0x20,4)
	};

	for ( ; n > 0; out += 3, ++in1, ++in2, ++in3, --n )
	  {	bits32 b24 =
		  tab3x1[*in1] | (tab3x1[*in2] >> 1) | (tab3x1[*in3] >> 2);
		out[0] = (byte)(b24 >> 16);
		out[1] = (byte)(b24 >> 8);
		out[2] = (byte)b24;
	  }
}

/* Convert 3Mx2 to 3x2. */
void
flip3x2(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	uint n = nbytes;
	static const far_data bits32 tab3x2[256] = {
		vtab(0x800000,0x400000,0x20000,0x10000,0x800,0x400,0x20,0x10)
	};

	for ( ; n > 0; out += 3, ++in1, ++in2, ++in3, --n )
	  {	bits32 b24 =
		  tab3x2[*in1] | (tab3x2[*in2] >> 2) | (tab3x2[*in3] >> 4);
		out[0] = (byte)(b24 >> 16);
		out[1] = (byte)(b24 >> 8);
		out[2] = (byte)b24;
	  }
}

/* Convert 3Mx4 to 3x4. */
void
flip3x4(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 3, ++in1, ++in2, ++in3, --n )
	  {	byte_var b1 = *in1, b2 = *in2, b3 = *in3;
		out[0] = (b1 & 0xf0) | (b2 >> 4);
		out[1] = (b3 & 0xf0) | (b1 & 0xf);
		out[2] = (byte)(b2 << 4) | (b3 & 0xf);
	  }
}

/* Convert 3Mx8 to 3x8. */
void
flip3x8(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 3, ++in1, ++in2, ++in3, --n )
	  {	out[0] = *in1;
		out[1] = *in2;
		out[2] = *in3;
	  }
}

/* Convert 3Mx12 to 3x12. */
void
flip3x12(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *pa = planes[0] + offset;
	const byte *pb = planes[1] + offset;
	const byte *pc = planes[2] + offset;
	uint n = nbytes;

	/* We are guaranteed that the input is an integral number of pixels. */
	/* This implies that n = 0 mod 3. */

	for ( ; n > 0; out += 9, pa += 3, pb += 3, pc += 3, n -= 3 )
	  {	byte_var a1 = pa[1], b0 = pb[0], b1 = pb[1],
		  b2 = pb[2], c1 = pc[1];
		out[0] = pa[0];
		out[1] = (a1 & 0xf0) | (b0 >> 4);
		out[2] = (byte)((b0 << 4) | (b1 >> 4));
		out[3] = pc[0];
		out[4] = (c1 & 0xf0) | (a1 & 0xf);
		out[5] = pa[2];
		out[6] = (byte)((b1 << 4) | (b2 >> 4));
		out[7] = (byte)((b2 << 4) | (c1 & 0xf));
		out[8] = pc[2];
	  }
}

/* Convert 4Mx1 to 4x1. */
void
flip4x1(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	const byte *in4 = planes[3] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 4, ++in1, ++in2, ++in3, ++in4, --n )
	  {	byte_var b1 = *in1, b2 = *in2, b3 = *in3, b4 = *in4;
		byte_var temp;

		/* Transpose blocks of 1 */
		transpose(b1, b2, 0x55, 1);
		transpose(b3, b4, 0x55, 1);
		/* Transpose blocks of 2 */
		transpose(b1, b3, 0x33, 2);
		transpose(b2, b4, 0x33, 2);
		/* There's probably a faster way to do this.... */
		out[0] = (b1 & 0xf0) | (b2 >> 4);
		out[1] = (b3 & 0xf0) | (b4 >> 4);
		out[2] = (byte)((b1 << 4) | (b2 & 0xf));
		out[3] = (byte)((b3 << 4) | (b4 & 0xf));
	  }
}

/* Convert 4Mx2 to 4x2. */
void
flip4x2(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	const byte *in4 = planes[3] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 4, ++in1, ++in2, ++in3, ++in4, --n )
	  {	byte_var b1 = *in1, b2 = *in2, b3 = *in3, b4 = *in4;
		byte_var temp;

		/* Transpose blocks of 4x2 */
		transpose(b1, b3, 0x0f, 4);
		transpose(b2, b4, 0x0f, 4);
		/* Transpose blocks of 2x1 */
		transpose(b1, b2, 0x33, 2);
		transpose(b3, b4, 0x33, 2);
		out[0] = b1;
		out[1] = b2;
		out[2] = b3;
		out[3] = b4;
	  }
}

/* Convert 4Mx4 to 4x4. */
void
flip4x4(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	const byte *in4 = planes[3] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 4, ++in1, ++in2, ++in3, ++in4, --n )
	  {	byte_var b1 = *in1, b2 = *in2, b3 = *in3, b4 = *in4;
		out[0] = (b1 & 0xf0) | (b2 >> 4);
		out[1] = (b3 & 0xf0) | (b4 >> 4);
		out[2] = (byte)((b1 << 4) | (b2 & 0xf));
		out[3] = (byte)((b3 << 4) | (b4 & 0xf));
	  }
}

/* Convert 4Mx8 to 4x8. */
void
flip4x8(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *in1 = planes[0] + offset;
	const byte *in2 = planes[1] + offset;
	const byte *in3 = planes[2] + offset;
	const byte *in4 = planes[3] + offset;
	uint n = nbytes;

	for ( ; n > 0; out += 4, ++in1, ++in2, ++in3, ++in4, --n )
	  {	out[0] = *in1;
		out[1] = *in2;
		out[2] = *in3;
		out[3] = *in4;
	  }
}

/* Convert 4Mx12 to 3x12. */
void
flip4x12(byte *buffer, const byte **planes, uint offset, uint nbytes)
{	byte *out = buffer;
	const byte *pa = planes[0] + offset;
	const byte *pb = planes[1] + offset;
	const byte *pc = planes[2] + offset;
	const byte *pd = planes[3] + offset;
	uint n = nbytes;

	/* We are guaranteed that the input is an integral number of pixels. */
	/* This implies that n = 0 mod 3. */

	for ( ; n > 0; out += 12, pa += 3, pb += 3, pc += 3, pd += 3, n -= 3 )
	  {	byte_var a1 = pa[1], b1 = pb[1], c1 = pc[1], d1 = pd[1];
		{ byte_var v0;
		  out[0] = pa[0];
		  v0 = pb[0];
		  out[1] = (a1 & 0xf0) | (v0 >> 4);
		  out[2] = (byte)((v0 << 4) | (b1 >> 4));
		  out[3] = pc[0];
		  v0 = pd[0];
		  out[4] = (c1 & 0xf0) | (v0 >> 4);
		  out[5] = (byte)((v0 << 4) | (d1 >> 4));
		}
		{ byte_var v2;
		  v2 = pa[2];
		  out[6] = (byte)((a1 << 4) | (v2 >> 4));
		  out[7] = (byte)((v2 << 4) | (b1 & 0xf));
		  out[8] = pb[2];
		  v2 = pc[2];
		  out[9] = (byte)((c1 << 4) | (v2 >> 4));
		  out[10] = (byte)((v2 << 4) | (d1 & 0xf));
		  out[11] = pd[2];
		}
	  }
}

/* Flip data given number of planes and bits per pixel. */
void (*image_flip_procs[2][13])(P4(byte *, const byte **, uint, uint)) = {
  { 0, flip3x1, flip3x2, 0, flip3x4, 0, 0, 0, flip3x8, 0, 0, 0, flip3x12 },
  { 0, flip4x1, flip4x2, 0, flip4x4, 0, 0, 0, flip4x8, 0, 0, 0, flip4x12 }
};
