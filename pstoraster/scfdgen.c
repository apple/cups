/* Copyright (C) 1992, 1994 Aladdin Enterprises.  All rights reserved.
  
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

/* scfdgen.c */
/* Generate the CCITTFaxDecode tables */
#include "stdio_.h"	/* includes std.h */
#include "scf.h"
#include "malloc_.h"
#include "memory_.h"

typedef void (*cfd_node_proc)(P6(cfd_node *, cfd_node *,
				 uint, int, int, int));
typedef void (*cfd_enum_proc)(P4(cfd_node_proc,
				 cfd_node *, cfd_node *, int));
private void cfd_build_tree(P4(cfd_node *, cfd_enum_proc, int, FILE *));
private void cfd_enumerate_white(P4(cfd_node_proc,
				    cfd_node *, cfd_node *, int));
private void cfd_enumerate_black(P4(cfd_node_proc,
				    cfd_node *, cfd_node *, int));
private void cfd_enumerate_2d(P4(cfd_node_proc,
				 cfd_node *, cfd_node *, int));
private void cfd_enumerate_uncompressed(P4(cfd_node_proc,
					   cfd_node *, cfd_node *, int));

main()
{	FILE *out = fopen("scfdtab.c", "w");
	cfd_node area[1 << max(cfd_white_initial_bits, cfd_black_initial_bits)];
	fputs("/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved. */\n\n", out);
	fputs("/* This file was generated automatically.  It is governed by the same terms */\n", out);
	fputs("/* as the files scfetab.c and scfdgen.c from which it was derived. */\n", out);
	fputs("/* Consult those files for the licensing terms and conditions. */\n\n", out);
	fputs("/* scfdtab.c */\n", out);
	fputs("/* Tables for CCITTFaxDecode filter. */\n\n", out);
	fputs("#include \"std.h\"\n", out);
	fputs("#include \"scommon.h\"\t\t/* for scf.h */\n", out);
	fputs("#include \"scf.h\"\n\n", out);
	fputs("/* White decoding table. */\n", out);
	fputs("const cfd_node far_data cf_white_decode[] = {\n", out);
	cfd_build_tree(area, cfd_enumerate_white, cfd_white_initial_bits, out);
	fputs("\n};\n\n", out);
	fputs("/* Black decoding table. */\n", out);
	fputs("const cfd_node far_data cf_black_decode[] = {\n", out);
	cfd_build_tree(area, cfd_enumerate_black, cfd_black_initial_bits, out);
	fputs("\n};\n\n", out);
	fputs("/* 2-D decoding table. */\n", out);
	fputs("const cfd_node far_data cf_2d_decode[] = {\n", out);
	cfd_build_tree(area, cfd_enumerate_2d, cfd_2d_initial_bits, out);
	fputs("\n};\n\n", out);
	fputs("/* Uncompresssed decoding table. */\n", out);
	fputs("const cfd_node far_data cf_uncompressed_decode[] = {\n", out);
	cfd_build_tree(area, cfd_enumerate_uncompressed, cfd_uncompressed_initial_bits, out);
	fputs("\n};\n\n", out);
	fputs("/* Dummy executable code to pacify compilers. */\n", out);
	fputs("void\ncfd_dummy()\n{\n}\n", out);
	fclose(out);
	return 0;
}

/* Initialize first-level leaves, count second-level nodes. */
private void
cfd_count_nodes(cfd_node *tree, cfd_node *ignore_extn,
  uint code, int code_length, int run_length, int initial_bits)
{	if ( code_length <= initial_bits )
	{	/* Initialize one or more first-level leaves. */
		int sh = initial_bits - code_length;
		cfd_node *np = &tree[code << sh];
		int i;
		for ( i = 1 << sh; i > 0; i--, np++ )
			np->run_length = run_length,
			np->code_length = code_length;
	}
	else
	{	/* Note the need for a second level. */
		cfd_node *np = &tree[code >> (code_length - initial_bits)];
		np->code_length = max(np->code_length, code_length);
	}
}

/* Initialize second-level nodes. */
private void
cfd_init2_nodes(cfd_node *tree, cfd_node *extn,
  uint code, int code_length, int run_length, int initial_bits)
{	int xbits = code_length - initial_bits;
	int xrep;
	cfd_node *np1, *np2;
	int i;
	if ( xbits <= 0 ) return;
	np1 = &tree[code >> xbits];
	np2 = &extn[np1->run_length - (1 << initial_bits)];
	xrep = np1->code_length - code_length;
	i = 1 << xrep;
	np2 += (code & ((1 << xbits) - 1)) * i;
	for ( ; i > 0; i--, np2++ )
		np2->run_length = run_length,
		np2->code_length = xbits;
}

/* Enumerate all the relevant white or black codes. */
private void
cfd_enumerate_codes(cfd_node_proc proc, cfd_node *tree, cfd_node *extn,
  int initial_bits, const cfe_run *tt, const cfe_run *mut)
{	int i;
	const cfe_run *ep;
	for ( i = 0, ep = tt; i < 64; i++, ep++ )
	  (*proc)(tree, extn, ep->code, ep->code_length, i, initial_bits);
	for ( i = 1, ep = mut + 1; i < 41; i++, ep++ )
	  (*proc)(tree, extn, ep->code, ep->code_length, i << 6, initial_bits);
	(*proc)(tree, extn, cf1_run_uncompressed.code, cf1_run_uncompressed.code_length, run_uncompressed, initial_bits);
	(*proc)(tree, extn, 0, run_eol_code_length - 1, run_zeros, initial_bits);
}
private void
cfd_enumerate_white(cfd_node_proc proc, cfd_node *tree, cfd_node *extn,
  int initial_bits)
{	cfd_enumerate_codes(proc, tree, extn, initial_bits,
			    cf_white_termination, cf_white_make_up);
}
private void
cfd_enumerate_black(cfd_node_proc proc, cfd_node *tree, cfd_node *extn,
  int initial_bits)
{	cfd_enumerate_codes(proc, tree, extn, initial_bits,
			    cf_black_termination, cf_black_make_up);
}

/* Enumerate the 2-D codes. */
private void
cfd_enumerate_2d(cfd_node_proc proc, cfd_node *tree, cfd_node *extn,
  int initial_bits)
{	int i;
	const cfe_run *ep;
	(*proc)(tree, extn, cf2_run_pass.code, cf2_run_pass.code_length,
		run2_pass, initial_bits);
	(*proc)(tree, extn, cf2_run_horizontal.code, cf2_run_horizontal.code_length,
		run2_horizontal, initial_bits);
	for ( i = 0; i < countof(cf2_run_vertical); i++ )
	{	ep = &cf2_run_vertical[i];
		(*proc)(tree, extn, ep->code, ep->code_length, i, initial_bits);
	}
	(*proc)(tree, extn, cf2_run_uncompressed.code, cf2_run_uncompressed.code_length,
		run_uncompressed, initial_bits);
	(*proc)(tree, extn, 0, run_eol_code_length - 1, run_zeros, initial_bits);
}

/* Enumerate the uncompressed codes. */
private void
cfd_enumerate_uncompressed(cfd_node_proc proc, cfd_node *tree, cfd_node *extn,
  int initial_bits)
{	int i;
	const cfe_run *ep;
	for ( i = 0; i < countof(cf_uncompressed); i++ )
	{	ep = &cf_uncompressed[i];
		(*proc)(tree, extn, ep->code, ep->code_length, i, initial_bits);
	}
	for ( i = 0; i < countof(cf_uncompressed_exit); i++ )
	{	ep = &cf_uncompressed_exit[i];
		(*proc)(tree, extn, ep->code, ep->code_length, i, initial_bits);
	}
}

/* Build and write out the table. */
private void
cfd_build_tree(cfd_node *tree, cfd_enum_proc enum_proc, int initial_bits,
  FILE *f)
{	cfd_node *np;
	const char *prev = "";
	int i, next;
	cfd_node *extn;
	memset(tree, 0, sizeof(cfd_node) << initial_bits);
	/* Construct and write the first level of the tree. */
	(*enum_proc)(cfd_count_nodes, tree, (cfd_node *)0, initial_bits);
	next = 0;
	for ( i = 0, np = tree; i < 1 << initial_bits; i++, np++ )
	{ if ( np->code_length > initial_bits )		/* second level needed */
	  { np->run_length = next + (1 << initial_bits);
	    next += 1 << (np->code_length - initial_bits);
	  }
	  fprintf(f, "%s\t{ %d, %d }", prev, np->run_length, np->code_length);
	  prev = ",\n";
	}
	/* Construct and write the second level. */
	extn = (cfd_node *)malloc(sizeof(cfd_node) * next);
	for ( i = 0, np = extn; i < next; i++, np++ )
		np->run_length = run_error,
		np->code_length = 0;
	(*enum_proc)(cfd_init2_nodes, tree, extn, initial_bits);
	for ( i = 0, np = extn; i < next; i++, np++ )
	  fprintf(f, ",\n\t{ %d, %d }", np->run_length, np->code_length);
	free((char *)extn);
}
