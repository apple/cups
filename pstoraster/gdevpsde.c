/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gdevpsde.c,v 1.1 2000/03/08 23:14:25 mike Exp $ */
/* Embedded font writing */
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfixed.h"
#include "gxfont.h"
#include "gxfont1.h"
#include "stream.h"
#include "gdevpstr.h"
#include "gdevpsdf.h"

private int
embed_table(gs_param_list * plist, const char *key, const float *values,
	    int count)
{
    if (count != 0) {
	gs_param_float_array fa;

	fa.size = count;
	fa.data = values;
	return param_write_float_array(plist, key, &fa);
    }
    return 0;
}

private void
embed_uid(stream * s, const gs_uid * puid)
{
    if (uid_is_UniqueID(puid))
	pprintld1(s, "/UniqueID %ld def\n", puid->id);
    else if (uid_is_XUID(puid)) {
	uint i, n = uid_XUID_size(puid);

	pputs(s, "/XUID [");
	for (i = 0; i < n; ++i)
	    pprintld1(s, "%ld ", uid_XUID_values(puid)[i]);
	pputs(s, "] def\n");
    }
}

/* Write an embedded Type 1 font. */
int
psdf_embed_type1_font(stream * s, gs_font_type1 * pfont)
{
    const gs_type1_data *const pdata = &pfont->data;
    gs_param_list *plist;
    param_printer_params_t ppp;
    int code;

    ppp = param_printer_params_default;
    ppp.item_suffix = " def\n";
    code = psdf_alloc_param_printer(&plist, &ppp, s,
				    print_binary_ok, s->memory);
    if (code < 0)
	return 0;

    /* Write the font header. */

    pputs(s, "%!PS-AdobeFont-1.0: ");
    pwrite(s, pfont->font_name.chars, pfont->font_name.size);
    pputs(s, "\n11 dict begin\n");

    /* Write FontInfo.  Currently we don't write anything there. */

    pputs(s, "/FontInfo 1 dict dup begin\n");
    pputs(s, "end readonly def\n");

    /* Write the main font dictionary. */

    pputs(s, "/FontName /");
    pwrite(s, pfont->font_name.chars, pfont->font_name.size);
    pputs(s, " def\n");
    pputs(s, "/Encoding ");
    switch (pfont->encoding_index) {
	case 0:
	    pputs(s, "StandardEncoding");
	    break;
	case 1:
	    pputs(s, "ISOLatin1Encoding");
	    break;
	default:{
		gs_char i;

		pputs(s, "256 array\n");
		pputs(s, "0 1 255 {1 index exch /.notdef put} for\n");
		for (i = 0; i < 256; ++i) {
		    gs_glyph glyph =
		    (*pfont->procs.encode_char) (NULL, (gs_font *) pfont, &i);
		    const char *namestr;
		    uint namelen;

		    if (glyph != gs_no_glyph &&
			(namestr = (*pfont->procs.callbacks.glyph_name) (glyph, &namelen)) != 0 &&
			!(namelen == 7 && !memcmp(namestr, ".notdef", 7))
			) {
			pprintd1(s, "dup %d /", (int)i);
			pwrite(s, namestr, namelen);
			pputs(s, " put\n");
		    }
		}
		pputs(s, "readonly");
	    }
    }
    pputs(s, " def\n");
    pprintg6(s, "/FontMatrix [%g %g %g %g %g %g] readonly def\n",
	     pfont->FontMatrix.xx, pfont->FontMatrix.xy,
	     pfont->FontMatrix.yx, pfont->FontMatrix.yy,
	     pfont->FontMatrix.tx, pfont->FontMatrix.ty);
    embed_uid(s, &pfont->UID);
    pprintg4(s, "/FontBBox {%g %g %g %g} readonly def\n",
	     pfont->FontBBox.p.x, pfont->FontBBox.p.y,
	     pfont->FontBBox.q.x, pfont->FontBBox.q.y);
    {
	private const gs_param_item_t font_items[] =
	{
	    {"FontType", gs_param_type_int,
	     offset_of(gs_font_type1, FontType)},
	    {"PaintType", gs_param_type_int,
	     offset_of(gs_font_type1, PaintType)},
	    {"StrokeWidth", gs_param_type_float,
	     offset_of(gs_font_type1, StrokeWidth)},
	    gs_param_item_end
	};

	code = gs_param_write_items(plist, pfont, NULL, font_items);
	if (code < 0)
	    return code;
    }
    pputs(s, "currentdict end\n");

    /* Write the Private dictionary. */

    pputs(s, "dup /Private 17 dict dup begin\n");
    pputs(s, "/-|{string currentfile exch readstring pop}executeonly def\n");
    pputs(s, "/|-{noaccess def}executeonly def\n");
    pputs(s, "/|{noaccess put}executeonly def\n");
    {
	private const gs_param_item_t private_items[] =
	{
	    {"lenIV", gs_param_type_int,
	     offset_of(gs_type1_data, lenIV)},
	    {"BlueFuzz", gs_param_type_int,
	     offset_of(gs_type1_data, BlueFuzz)},
	    {"BlueScale", gs_param_type_float,
	     offset_of(gs_type1_data, BlueScale)},
	    {"BlueShift", gs_param_type_float,
	     offset_of(gs_type1_data, BlueShift)},
	    {"ExpansionFactor", gs_param_type_float,
	     offset_of(gs_type1_data, ExpansionFactor)},
	    {"ForceBold", gs_param_type_bool,
	     offset_of(gs_type1_data, ForceBold)},
	    {"LanguageGroup", gs_param_type_int,
	     offset_of(gs_type1_data, LanguageGroup)},
	    {"RndStemUp", gs_param_type_bool,
	     offset_of(gs_type1_data, RndStemUp)},
	    gs_param_item_end
	};
	gs_type1_data defaults;

	defaults.lenIV = 4;
	defaults.BlueFuzz = 1;
	defaults.BlueScale = 0.039625;
	defaults.BlueShift = 7.0;
	defaults.ExpansionFactor = 0.06;
	defaults.ForceBold = false;
	defaults.LanguageGroup = 0;
	defaults.RndStemUp = true;
	code = gs_param_write_items(plist, pdata, &defaults, private_items);
	if (code < 0)
	    return code;
	embed_table(plist, "BlueValues", pdata->BlueValues.values,
		    pdata->BlueValues.count);
	embed_table(plist, "OtherBlues", pdata->OtherBlues.values,
		    pdata->OtherBlues.count);
	embed_table(plist, "FamilyBlues", pdata->FamilyBlues.values,
		    pdata->FamilyBlues.count);
	embed_table(plist, "FamilyOtherBlues", pdata->FamilyOtherBlues.values,
		    pdata->FamilyOtherBlues.count);
	embed_table(plist, "StdHW", pdata->StdHW.values,
		    pdata->StdHW.count);
	embed_table(plist, "StemSnapH", pdata->StemSnapH.values,
		    pdata->StemSnapH.count);
	embed_table(plist, "StemSnapV", pdata->StemSnapV.values,
		    pdata->StemSnapV.count);
    }
    embed_uid(s, &pfont->UID);
    pputs(s, "/MinFeature{16 16} |-\n");
    pputs(s, "/password 5839 def\n");

    /* Write the Subrs. */

    {
	int n, i;
	gs_const_string str;

	for (n = 0;
	     (*pdata->procs->subr_data) (pfont, n, false, &str) !=
	     gs_error_rangecheck;
	    )
	    ++n;
	pprintd1(s, "/Subrs %d array\n", n);
	for (i = 0; i < n; ++i)
	    if ((*pdata->procs->subr_data) (pfont, i, false, &str) >= 0) {
		char buf[50];

		sprintf(buf, "dup %d %u -| ", i, str.size);
		pputs(s, buf);
		pwrite(s, str.data, str.size);
		pputs(s, " |\n");
	    }
	pputs(s, "|-\n");
    }

    /* We don't write OtherSubrs -- there had better not be any! */

    /* Write the CharStrings. */

    {
	int num_chars = 0;
	gs_glyph glyph;
	int index = 0;
	gs_const_string gdata;
	int code;

	for (glyph = gs_no_glyph, index = 0;
	     code = (*pdata->procs->next_glyph) (pfont, &index, &glyph),
	     index != 0;
	    )
	    if (code == 0 && (*pdata->procs->glyph_data) (pfont, glyph, &gdata) >= 0)
		++num_chars;
	pprintd1(s, "2 index /CharStrings %d dict dup begin\n", num_chars);
	for (glyph = gs_no_glyph, index = 0;
	     code = (*pdata->procs->next_glyph) (pfont, &index, &glyph),
	     index != 0;
	    )
	    if (code == 0 && (*pdata->procs->glyph_data) (pfont, glyph, &gdata) >= 0) {
		uint gssize;
		const char *gstr =
		(*pfont->procs.callbacks.glyph_name) (glyph, &gssize);

		pputs(s, "/");
		pwrite(s, gstr, gssize);
		pprintd1(s, " %d -| ", gdata.size);
		pwrite(s, gdata.data, gdata.size);
		pputs(s, " |-\n");
	    }
    }

    /* Wrap up. */

    pputs(s, "end\nend\nreadonly put\nnoaccess put\n");
    pputs(s, "dup/FontName get exch definefont pop\n");
    psdf_free_param_printer(plist);
    return 0;
}
