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

/* gdevpdft.c */
/* Text handling for PDF-writing driver. */
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"			/* for bytes_compare */
#include "gdevpdfx.h"
#include "strimpl.h"
#include "sstring.h"			/* for PSStringEncode */

/*
 * Define whether to re-encode characters in order to find them within base
 * font encodings.  This can greatly reduce the number of characters
 * represented as bitmaps, but it may cause the text in the PDF file to
 * differ from the text in the PostScript input.
 */
#define RE_ENCODE_OK

/*
 * The show pseudo-operator is currently the way that the PostScript code
 * passes show operations to the PDF writer.  It is a hack!  Its "value"
 * is the following:
 *	string, [cx cy char ax ay px py], scalematrix, fontname, [R G B],
 *	  encoding, baseencoding
 * Note that all coordinates and distances are floating point values in
 * device space.  The scalematrix is the concatenation of
 *	FontMatrix
 *	inverse of base FontMatrix
 *	CTM
 * This represents the transformation from a 1-unit-based character space
 * to device space.  The base encoding is StandardEncoding for all fonts
 * except Symbol and ZapfDingbats.
 */

/* Define the 14 standard built-in fonts. */
private const char *standard_font_names[] = {
  "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
  "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique",
  "Symbol",
  "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
  "ZapfDingbats",
  0
};

/* Begin text. */
/* Return an error if the page has too many contents parts. */
private int
pdf_begin_text(gx_device_pdf *pdev)
{	switch ( pdev->in_contents )
	{
	case 0:
	{	int code = pdf_begin_contents(pdev);
		if ( code < 0 )
		  return code;
	}
	case 1:
		fputs("BT\n", pdev->file);
		pdev->in_contents = 2;
		pdev->procsets |= Text;
	case 2:
		;
	}
	return 0;
}

/* Process a show operation. */
int
pdfshow_process(gx_device_pdf *pdev, const gs_param_dict *ptd)
{
#define plist (ptd->list)
	gs_param_string str, fnstr;
	gs_param_float_array va;
#define v_cx va.data[0]
#define v_cy va.data[1]
#define v_cch (int)va.data[2]
#define v_ax va.data[3]
#define v_ay va.data[4]
#define v_px va.data[5]
#define v_py va.data[6]
	gs_param_float_array ma;
#define cmat (*(const gs_matrix *)ma.data)
	gs_param_float_array ca;
#define c_fr ca.data[0]
#define c_fg ca.data[1]
#define c_fb ca.data[2]
	gs_param_string_array ea, bea;
	int code;
	pdf_font *ppf;
	FILE *file = pdev->file;
	double sx = pdev->scale.x, sy = pdev->scale.y;
	bool re_encode = false;
	float size;
	byte strbuf[200];

	if ( (code = param_read_string(plist, "String", &str)) ||
	     (code = param_read_float_array(plist, "Values", &va)) ||
	     va.size != 7 ||
	     (code = param_read_float_array(plist, "Matrix", &ma)) ||
	     ma.size != 6 ||
	     (code = param_read_string(plist, "FontName", &fnstr)) ||
	     (code = param_read_float_array(plist, "Color", &ca)) ||
	     ca.size != 3 ||
	     (code = param_read_name_array(plist, "Encoding", &ea)) ||
	     ea.size != 256 ||
	     (code = param_read_name_array(plist, "BaseEncoding", &bea)) ||
	     bea.size != 256
	   )
	  return_error(gs_error_rangecheck);
	if ( v_cy != 0 || (v_cch != 32 && v_cx != 0) || v_ay != 0 )
	  return_error(gs_error_undefined);
	/* Check that all characters match the base encoding. */
	{ uint i;
	  for ( i = 0; i < str.size; ++i )
	    { byte chr = str.data[i];
	      if ( ea.data[chr].data != bea.data[chr].data )
		{
#ifdef RE_ENCODE_OK
		  /* Since the penalty for converting text to a bitmap */
		  /* is so severe, see if the character is present */
		  /* at some other position in the base encoding. */
		  int ei;
		  for ( ei = 0; ei < 256; ++ei )
		    if ( ea.data[chr].data == bea.data[ei].data )
		      break;
		  if ( ei == 256 )
		    return_error(gs_error_undefined);
		  /* It really simplifies things if we can buffer */
		  /* the entire string locally in one piece.... */
		  if ( !re_encode )
		    { if ( str.size > sizeof(strbuf) )
		        return_error(gs_error_limitcheck);
		      memcpy(strbuf, str.data, str.size);
		      re_encode = true;
		    }
		  strbuf[i] = (byte)ei;
#else
		  return_error(gs_error_undefined);
#endif
		}
	    }
	}
	/* Find or create the font resource. */
	for ( ppf = (pdf_font *)pdev->resources[resourceFont]; ppf != 0;
	      ppf = ppf->next
	    )
	  if ( !bytes_compare(ppf->fname.data, ppf->fname.size,
			      fnstr.data, fnstr.size)
	     )
	    break;
	size = (cmat.xx != 0 ? cmat.xx / sx : 1);
	if ( ppf == 0 )
	{	/* Currently, we only handle the built-in fonts. */
		const char **ppfn;
		for ( ppfn = standard_font_names; *ppfn; ++ppfn )
		  if ( strlen(*ppfn) == fnstr.size &&
		       !strncmp(*ppfn, (const char *)fnstr.data, fnstr.size)
		     )
		    break;
		if ( !*ppfn )
		  return_error(gs_error_undefined);
		code = pdf_begin_resource(pdev, resourceFont,
					  (pdf_resource **)&ppf);
		if ( code < 0 )
		  return_error(gs_error_undefined);
		fprintf(file, " /Subtype /Type1 /BaseFont /%s >>\n",
			*ppfn);
		ppf->fname.data = fnstr.data, ppf->fname.size = fnstr.size;
		pdf_end_resource(pdev);
	}
	code = pdf_begin_text(pdev);
	if ( code < 0 )
	  return code;
	code = pdf_set_color(pdev,
			(*dev_proc(pdev, map_rgb_color))((gx_device *)pdev,
				(gx_color_value)(c_fr * gx_max_color_value),
				(gx_color_value)(c_fg * gx_max_color_value),
				(gx_color_value)(c_fb * gx_max_color_value)),
			     &pdev->fill_color, "rg");
	if ( code < 0 )
	  return code;
	/* We attempt to eliminate redundant parameter settings. */
	if ( ppf != pdev->text_state.font || size != pdev->text_state.size )
	  { fprintf(file, "/R%ld ", ppf->id);
	    gprintf1(file, "%g Tf\n", size);
	    pdev->text_state.font = ppf;
	    pdev->text_state.size = size;
	  }
	sx *= size;
	sy *= size;
	{ float chars = v_cx / sx;
	  if ( pdev->text_state.character_spacing != chars )
	    { gprintf1(file, "%g Tc\n", chars);
	      pdev->text_state.character_spacing = chars;
	    }
	}
	{ float words = v_ax / sx;
	  if ( pdev->text_state.word_spacing != words )
	    { gprintf1(file, "%g Tw\n", words);
	      pdev->text_state.word_spacing = words;
	    }
	}
	gprintf6(file, "%g %g %g %g %g %g Tm\n",
		 cmat.xx / sx, cmat.xy / sy, cmat.yx / sx, cmat.yy / sy,
		 (v_px + cmat.tx) / pdev->scale.x,
		 (v_py + cmat.ty) / pdev->scale.y);
	/* Write the string.  Make sure it gets any necessary \s. */
	fputc('(', file);
	{ byte buf[100];		/* size is arbitrary */
	  stream_cursor_read r;
	  stream_cursor_write w;
	  int status;

	  r.ptr = (re_encode ? strbuf : str.data) - 1;
	  r.limit = r.ptr + str.size;
	  w.limit = buf + sizeof(buf) - 1;
	  do
	    { w.ptr = buf - 1;
	      status = (*s_PSSE_template.process)(NULL, &r, &w, true);
	      fwrite(buf, 1, (uint)(w.ptr + 1 - buf), file);
	    }
	  while ( status == 1 );
	}
	fputs(" Tj\n", file);

	return 0;
}
