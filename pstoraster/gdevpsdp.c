/*
  Copyright 1993-2000 by Easy Software Products.
  Copyright 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
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

/*$Id: gdevpsdp.c,v 1.3 2000/05/17 14:25:16 mike Exp $ */
/* (Distiller) parameter handling for PostScript and PDF writers */
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gdevpsdf.h"
#include "gdevpstr.h"
#include "strimpl.h"		/* for short-sighted compilers */
#include "scfx.h"
#include <config.h>
#ifdef HAVE_LIBJPEG
#include "jpeglib.h"		/* for sdct.h */
#include "sdct.h"
#endif /* HAVE_LIBJPEG */
#include "slzwx.h"
#include "srlx.h"
#ifdef HAVE_LIBZ
#include "szlibx.h"
#endif /* HAVE_LIBZ */

/* ---------------- Get/put Distiller parameters ---------------- */

/*
 * This code handles all the Distiller parameters except the *ACSDict and
 * *ImageDict parameter dictionaries.  (It doesn't cause any of the
 * parameters actually to have any effect.)
 */

typedef struct psdf_image_filter_name_s {
    const char *pname;
    const stream_template *template;
    psdf_version min_version;
} psdf_image_filter_name;
typedef struct psdf_image_param_names_s {
    const char *ACSDict;	/* not used for mono */
    const char *AntiAlias;
    const char *AutoFilter;	/* not used for mono */
    const char *Depth;
    const char *Dict;
    const char *Downsample;
    const char *DownsampleType;
    const char *Encode;
    const char *Filter;
    const char *Resolution;
} psdf_image_param_names;
private const psdf_image_param_names Color_names =
{
    "ColorACSImageDict", "AntiAliasColorImages", "AutoFilterColorImages",
    "ColorImageDepth", "ColorImageDict",
    "DownsampleColorImages", "ColorImageDownsampleType", "EncodeColorImages",
    "ColorImageFilter", "ColorImageResolution"
};
private const psdf_image_filter_name Poly_filters[] =
{
#ifdef HAVE_LIBJPEG
    {"DCTEncode", &s_DCTE_template},
#endif /* HAVE_LIBJPEG */
#ifdef HAVE_LIBZ
    {"FlateEncode", &s_zlibE_template, psdf_version_ll3},
#endif /* HAVE_LIBZ */
    {"LZWEncode", &s_LZWE_template},
    {0, 0}
};
private const psdf_image_param_names Gray_names =
{
    "GrayACSImageDict", "AntiAliasGrayImages", "AutoFilterGrayImages",
    "GrayImageDepth", "GrayImageDict",
    "DownsampleGrayImages", "GrayImageDownsampleType", "EncodeGrayImages",
    "GrayImageFilter", "GrayImageResolution"
};
private const psdf_image_param_names Mono_names =
{
    0, "AntiAliasMonoImages", 0,
    "MonoImageDepth", "MonoImageDict",
    "DownsampleMonoImages", "MonoImageDownsampleType", "EncodeMonoImages",
    "MonoImageFilter", "MonoImageResolution"
};
private const psdf_image_filter_name Mono_filters[] =
{
    {"CCITTFaxEncode", &s_CFE_template},
#ifdef HAVE_LIBZ
    {"FlateEncode", &s_zlibE_template, psdf_version_ll3},
#endif /* HAVE_LIBZ */
    {"LZWEncode", &s_LZWE_template},
    {"RunLengthEncode", &s_RLE_template},
    {0, 0}
};
private const char *const AutoRotatePages_names[] =
{
    "None", "All", "PageByPage", 0
};
private const char *const ColorConversionStrategy_names[] =
{
    "LeaveColorUnchanged", "UseDeviceDependentColor",
    "UseDeviceIndependentColor", 0
};
private const char *const DownsampleType_names[] =
{
    "Average", "Subsample", 0
};
private const char *const TransferFunctionInfo_names[] =
{
    "Preserve", "Apply", "Remove", 0
};
private const char *const UCRandBGInfo_names[] =
{
    "Preserve", "Remove", 0
};

/* -------- Get parameters -------- */

#ifdef HAVE_LIBJPEG
extern stream_state_proc_get_params(s_DCTE_get_params, stream_DCT_state);
#endif /* HAVE_LIBJPEG */
extern stream_state_proc_get_params(s_CF_get_params, stream_CF_state);
typedef stream_state_proc_get_params((*ss_get_params_t), stream_state);

private int
psdf_CF_get_params(gs_param_list * plist, const stream_state * ss, bool all)
{
    return (ss == 0 ? 0 :
	    s_CF_get_params(plist, (const stream_CF_state *)ss, all));
}
#ifdef HAVE_LIBJPEG
private int
psdf_DCT_get_params(gs_param_list * plist, const stream_state * ss, bool all)
{
    int code = (ss == 0 ? 0 :
		s_DCTE_get_params(plist, (const stream_DCT_state *)ss, all));
    /*
     * Add dummy Columns, Rows, and Colors parameters so that put_params
     * won't complain.
     */
    int dummy_size = 8, dummy_colors = 3;

    if (code < 0 ||
	(code = param_write_int(plist, "Columns", &dummy_size)) < 0 ||
	(code = param_write_int(plist, "Rows", &dummy_size)) < 0 ||
	(code = param_write_int(plist, "Colors", &dummy_colors)) < 0
	)
	return code;
    return 0;
}
#endif /* HAVE_LIBJPEG */

/*
 * Get an image Dict parameter.  Note that we return a default (usually
 * empty) dictionary if the parameter has never been set.
 */
private int
psdf_get_image_dict_param(gs_param_list * plist, const gs_param_name pname,
			  stream_state * ss, ss_get_params_t get_params)
{
    gs_param_dict dict;
    int code;

    if (pname == 0)
	return 0;
    dict.size = 12;		/* enough for all param dicts we know about */
    if ((code = param_begin_write_dict(plist, pname, &dict, false)) < 0)
	return code;
    code = (*get_params)(dict.list, ss, false);
    param_end_write_dict(plist, pname, &dict);
    return code;
}

/* Get a set of image-related parameters. */
private int
psdf_get_image_params(gs_param_list * plist,
	  const psdf_image_param_names * pnames, psdf_image_params * params)
{
    int code;
    gs_param_string dsts, fs;

    param_string_from_string(dsts,
			     DownsampleType_names[params->DownsampleType]);
    if (
#ifdef HAVE_LIBJPEG
	   (code = psdf_get_image_dict_param(plist, pnames->ACSDict,
					     params->ACSDict,
					     psdf_DCT_get_params)) < 0 ||
#endif /* HAVE_LIBJPEG */
	   (code = param_write_bool(plist, pnames->AntiAlias,
				    &params->AntiAlias)) < 0 ||
	   (pnames->AutoFilter != 0 &&
	    (code = param_write_bool(plist, pnames->AutoFilter,
				     &params->AutoFilter)) < 0) ||
	   (code = param_write_int(plist, pnames->Depth,
				   &params->Depth)) < 0 ||
	   (code = psdf_get_image_dict_param(plist, pnames->Dict,
					     params->Dict,
#ifdef HAVE_LIBJPEG
					     (params->Dict == 0 ||
					      params->Dict->template ==
					      &s_CFE_template ?
					      psdf_CF_get_params :
					      psdf_DCT_get_params))) < 0 ||
#else
					      psdf_CF_get_params)) < 0 ||
#endif /* HAVE_LIBJPEG */
	   (code = param_write_bool(plist, pnames->Downsample,
				    &params->Downsample)) < 0 ||
	   (code = param_write_name(plist, pnames->DownsampleType,
				    &dsts)) < 0 ||
	   (code = param_write_bool(plist, pnames->Encode,
				    &params->Encode)) < 0 ||
	   (code = (params->Filter == 0 ? 0 :
		    (param_string_from_string(fs, params->Filter),
		     param_write_name(plist, pnames->Filter, &fs)))) < 0 ||
	   (code = param_write_int(plist, pnames->Resolution,
				   &params->Resolution)) < 0
	)
	DO_NOTHING;
    return code;
}

/* Get parameters. */
int
gdev_psdf_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_psdf *pdev = (gx_device_psdf *) dev;
    int code = gdev_vector_get_params(dev, plist);
    gs_param_string arps, ccss, tfis, ucrbgis;

    if (code < 0)
	return code;
    param_string_from_string(arps,
		  AutoRotatePages_names[(int)pdev->params.AutoRotatePages]);
    param_string_from_string(ccss,
			     ColorConversionStrategy_names[(int)pdev->params.ColorConversionStrategy]);
    param_string_from_string(tfis,
	TransferFunctionInfo_names[(int)pdev->params.TransferFunctionInfo]);
    param_string_from_string(ucrbgis,
			UCRandBGInfo_names[(int)pdev->params.UCRandBGInfo]);
    if (
    /* General parameters */

	   (code = param_write_bool(plist, "ASCII85EncodePages",
				    &pdev->params.ASCII85EncodePages)) < 0 ||
	   (code = param_write_name(plist, "AutoRotatePages",
				    &arps)) < 0 ||
	   (code = param_write_bool(plist, "CompressPages",
				    &pdev->params.CompressPages)) < 0 ||
	   (code = param_write_long(plist, "ImageMemory",
				    &pdev->params.ImageMemory)) < 0 ||
	   (code = param_write_bool(plist, "LZWEncodePages",
				    &pdev->params.LZWEncodePages)) < 0 ||
	   (code = param_write_bool(plist, "PreserveHalftoneInfo",
				 &pdev->params.PreserveHalftoneInfo)) < 0 ||
	   (code = param_write_bool(plist, "PreserveOPIComments",
				  &pdev->params.PreserveOPIComments)) < 0 ||
	   (code = param_write_bool(plist, "PreserveOverprintSettings",
			    &pdev->params.PreserveOverprintSettings)) < 0 ||
      (code = param_write_name(plist, "TransferFunctionInfo", &tfis)) < 0 ||
	   (code = param_write_name(plist, "UCRandBGInfo", &ucrbgis)) < 0 ||
	   (code = param_write_bool(plist, "UseFlateCompression",
				  &pdev->params.UseFlateCompression)) < 0 ||

    /* Color sampled image parameters */

	   (code = psdf_get_image_params(plist, &Color_names, &pdev->params.ColorImage)) < 0 ||
	   (code = param_write_name(plist, "ColorConversionStrategy",
				    &ccss)) < 0 ||
	   (code = param_write_bool(plist, "ConvertCMYKImagesToRGB",
			       &pdev->params.ConvertCMYKImagesToRGB)) < 0 ||
	   (code = param_write_bool(plist, "ConvertImagesToIndexed",
			       &pdev->params.ConvertImagesToIndexed)) < 0 ||

    /* Gray sampled image parameters */

	   (code = psdf_get_image_params(plist, &Gray_names, &pdev->params.GrayImage)) < 0 ||

    /* Mono sampled image parameters */

	   (code = psdf_get_image_params(plist, &Mono_names, &pdev->params.MonoImage)) < 0 ||

    /* Font embedding parameters */

	   (code = param_write_name_array(plist, "AlwaysEmbed", &pdev->params.AlwaysEmbed)) < 0 ||
	   (code = param_write_name_array(plist, "NeverEmbed", &pdev->params.NeverEmbed)) < 0 ||
	   (code = param_write_bool(plist, "EmbedAllFonts", &pdev->params.EmbedAllFonts)) < 0 ||
	   (code = param_write_bool(plist, "SubsetFonts", &pdev->params.SubsetFonts)) < 0 ||
	   (code = param_write_int(plist, "MaxSubsetPct", &pdev->params.MaxSubsetPct)) < 0
	);
    return code;
}

/* -------- Put parameters -------- */

#ifdef HAVE_LIBJPEG
extern stream_state_proc_put_params(s_DCTE_put_params, stream_DCT_state);
#endif /* HAVE_LIBJPEG */
extern stream_state_proc_put_params(s_CF_put_params, stream_CF_state);
typedef stream_state_proc_put_params((*ss_put_params_t), stream_state);

private int
psdf_CF_put_params(gs_param_list * plist, stream_state * st)
{
    stream_CFE_state *const ss = (stream_CFE_state *) st;

    (*s_CFE_template.set_defaults) (st);
    ss->K = -1;
    ss->BlackIs1 = true;
    return s_CF_put_params(plist, (stream_CF_state *) ss);
}
#ifdef HAVE_LIBJPEG
private int
psdf_DCT_put_params(gs_param_list * plist, stream_state * ss)
{
    return s_DCTE_put_params(plist, (stream_DCT_state *) ss);
}
#endif /* HAVE_LIBJPEG */

/* Compare a C string and a gs_param_string. */
bool
psdf_key_eq(const gs_param_string * pcs, const char *str)
{
    return (strlen(str) == pcs->size &&
	    !strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Put an enumerated value. */
private int
psdf_put_enum_param(gs_param_list * plist, gs_param_name param_name,
		    int *pvalue, const char *const pnames[], int ecode)
{
    gs_param_string ens;
    int code = param_read_name(plist, param_name, &ens);

    switch (code) {
	case 1:
	    return ecode;
	case 0:
	    {
		int i;

		for (i = 0; pnames[i] != 0; ++i)
		    if (psdf_key_eq(&ens, pnames[i])) {
			*pvalue = i;
			return 0;
		    }
	    }
	    code = gs_error_rangecheck;
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, code);
    }
    return code;
}

/* Put a Boolean or integer parameter. */
int
psdf_put_bool_param(gs_param_list * plist, gs_param_name param_name,
		    bool * pval, int ecode)
{
    int code;

    switch (code = param_read_bool(plist, param_name, pval)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}
int
psdf_put_int_param(gs_param_list * plist, gs_param_name param_name,
		   int *pval, int ecode)
{
    int code;

    switch (code = param_read_int(plist, param_name, pval)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}

/* Put [~](Always|Never)Embed parameters. */
private int
psdf_put_embed_param(gs_param_list * plist, gs_param_name notpname,
		     gs_param_string_array * psa, int ecode)
{
    gs_param_name pname = notpname + 1;
    int code;
    gs_param_string_array nsa;

/***** Storage management is incomplete ******/
/***** Doesn't do incremental add/delete ******/
    switch (code = param_read_name_array(plist, pname, psa)) {
	default:
	    ecode = code;
	    param_signal_error(plist, pname, ecode);
	case 0:
	case 1:
	    break;
    }
    switch (code = param_read_name_array(plist, notpname, &nsa)) {
	default:
	    ecode = code;
	    param_signal_error(plist, notpname, ecode);
	case 0:
	case 1:
	    break;
    }
    return ecode;
}

/* Put an image Dict parameter. */
private int
psdf_put_image_dict_param(gs_param_list * plist, const gs_param_name pname,
		      stream_state ** pss, const stream_template * template,
			  ss_put_params_t put_params, gs_memory_t * mem)
{
    gs_param_dict dict;
    stream_state *ss = *pss;
    int code;

    switch (code = param_begin_read_dict(plist, pname, &dict, false)) {
	default:
	    param_signal_error(plist, pname, code);
	    return code;
	case 1:
	    ss = 0;
	    break;
	case 0:{
	    /******
	     ****** THIS CAUSES A SEGV FOR DCT FILTERS, BECAUSE
	     ****** THEY DON'T INTIALIZE PROPERLY.
	     ******/
#ifdef HAVE_LIBJPEG
	    if (template != &s_DCTE_template)
#endif /* HAVE_LIBJPEG */
	    {
		stream_state *ss_new =
		    s_alloc_state(mem, template->stype, pname);

		if (ss_new == 0)
		    return_error(gs_error_VMerror);
		ss_new->template = template;
		if (template->set_defaults)
		    (*template->set_defaults)(ss_new);
		code = (*put_params)(dict.list, ss_new);
		if (code < 0) {
		    param_signal_error(plist, pname, code);
		    /* Make sure we free the new state. */
		    *pss = ss_new;
		} else
		    ss = ss_new;
	    }
	    }
	    param_end_read_dict(plist, pname, &dict);
    }
    if (*pss != ss) {
	if (ss) {
	    /****** FREE SUBSIDIARY OBJECTS -- HOW? ******/
	    gs_free_object(mem, *pss, pname);
	}
	*pss = ss;
    }
    return code;
}

/* Put a set of image-related parameters. */
private int
psdf_put_image_params(const gx_device_psdf * pdev, gs_param_list * plist,
 const psdf_image_param_names * pnames, const psdf_image_filter_name * pifn,
		      psdf_image_params * params, int ecode)
{
    gs_param_string fs;

    /*
     * Since this procedure can be called before the device is open,
     * we must use pdev->memory rather than pdev->v_memory.
     */
    gs_memory_t *mem = pdev->memory;
    gs_param_name pname;
    int dsti = params->DownsampleType;
    int code;

#ifdef HAVE_LIBJPEG
    if ((pname = pnames->ACSDict) != 0) {
	code = psdf_put_image_dict_param(plist, pname, &params->ACSDict,
					 &s_DCTE_template,
					 psdf_DCT_put_params, mem);
	if (code < 0)
	    ecode = code;
    }
#endif /* HAVE_LIBJPEG */
    ecode = psdf_put_bool_param(plist, pnames->AntiAlias,
				&params->AntiAlias, ecode);
    if (pnames->AutoFilter)
	ecode = psdf_put_bool_param(plist, pnames->AutoFilter,
				    &params->AutoFilter, ecode);
    ecode = psdf_put_int_param(plist, pnames->Depth,
			       &params->Depth, ecode);
    if ((pname = pnames->Dict) != 0) {
	const stream_template *template;
	ss_put_params_t put_params;

	/* Hack to determine what kind of a Dict we want: */
	if (pnames->Dict[0] == 'M')
	    template = &s_CFE_template,
		put_params = psdf_CF_put_params;
#ifdef HAVE_LIBJPEG
	else
	    template = &s_DCTE_template,
		put_params = psdf_DCT_put_params;
#endif /* HAVE_LIBJPEG */
	code = psdf_put_image_dict_param(plist, pname, &params->Dict,
					 template, put_params, mem);
	if (code < 0)
	    ecode = code;
    }
    ecode = psdf_put_bool_param(plist, pnames->Downsample,
				&params->Downsample, ecode);
    if ((ecode = psdf_put_enum_param(plist, pnames->DownsampleType,
				     &dsti, DownsampleType_names,
				     ecode)) >= 0
	)
	params->DownsampleType = (enum psdf_downsample_type)dsti;
    ecode = psdf_put_bool_param(plist, pnames->Encode,
				&params->Encode, ecode);
    switch (code = param_read_string(plist, pnames->Filter, &fs)) {
	case 0:
	    {
		const psdf_image_filter_name *pn = pifn;

		while (pn->pname != 0 && !psdf_key_eq(&fs, pn->pname))
		    pn++;
		if (pn->pname == 0 || pn->min_version > pdev->version) {
		    ecode = gs_error_rangecheck;
		    goto ipe;
		}
		params->Filter = pn->pname;
		params->filter_template = pn->template;
		break;
	    }
	default:
	    ecode = code;
	  ipe:param_signal_error(plist, pnames->Filter, ecode);
	case 1:
	    break;
    }
    ecode = psdf_put_int_param(plist, pnames->Resolution,
			       &params->Resolution, ecode);
    if (ecode >= 0) {		/* Force parameters to acceptable values. */
	if (params->Resolution < 1)
	    params->Resolution = 1;
	switch (params->Depth) {
	    default:
		params->Depth = -1;
	    case 1:
	    case 2:
	    case 4:
	    case 8:
	    case -1:
		break;
	}
    }
    return ecode;
}

/* Put parameters. */
int
gdev_psdf_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_psdf *pdev = (gx_device_psdf *) dev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    psdf_distiller_params params;

    /* General parameters. */

    params = pdev->params;

    ecode = psdf_put_bool_param(plist, "ASCII85EncodePages",
				&params.ASCII85EncodePages, ecode);
    {
	int arpi = params.AutoRotatePages;

	ecode = psdf_put_enum_param(plist, "AutoRotatePages", &arpi,
				    AutoRotatePages_names, ecode);
	params.AutoRotatePages = (enum psdf_auto_rotate_pages)arpi;
    }
    ecode = psdf_put_bool_param(plist, "CompressPages",
				&params.CompressPages, ecode);
    switch (code = param_read_long(plist, (param_name = "ImageMemory"), &params.ImageMemory)) {
	default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
	    break;
    }
    ecode = psdf_put_bool_param(plist, "LZWEncodePages",
				&params.LZWEncodePages, ecode);
    ecode = psdf_put_bool_param(plist, "PreserveHalftoneInfo",
				&params.PreserveHalftoneInfo, ecode);
    ecode = psdf_put_bool_param(plist, "PreserveOPIComments",
				&params.PreserveOPIComments, ecode);
    ecode = psdf_put_bool_param(plist, "PreserveOverprintSettings",
				&params.PreserveOverprintSettings, ecode);
    {
	int tfii = params.TransferFunctionInfo;

	ecode = psdf_put_enum_param(plist, "TransferFunctionInfo", &tfii,
				    TransferFunctionInfo_names, ecode);
	params.TransferFunctionInfo = (enum psdf_transfer_function_info)tfii;
    }
    {
	int ucrbgi = params.UCRandBGInfo;

	ecode = psdf_put_enum_param(plist, "UCRandBGInfo", &ucrbgi,
				    UCRandBGInfo_names, ecode);
	params.UCRandBGInfo = (enum psdf_ucr_and_bg_info)ucrbgi;
    }
#ifdef HAVE_LIBZ
    ecode = psdf_put_bool_param(plist, "UseFlateCompression",
				&params.UseFlateCompression, ecode);
#endif /* HAVE_LIBZ */

    /* Color sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist, &Color_names, Poly_filters,
				  &params.ColorImage, ecode);
    {
	int ccsi = params.ColorConversionStrategy;

	ecode = psdf_put_enum_param(plist, "ColorConversionStrategy", &ccsi,
				    ColorConversionStrategy_names, ecode);
	params.ColorConversionStrategy =
	    (enum psdf_color_conversion_strategy)ccsi;
    }
    ecode = psdf_put_bool_param(plist, "ConvertCMYKImagesToRGB",
				&params.ConvertCMYKImagesToRGB, ecode);
    ecode = psdf_put_bool_param(plist, "ConvertImagesToIndexed",
				&params.ConvertImagesToIndexed, ecode);

    /* Gray sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist, &Gray_names, Poly_filters,
				  &params.GrayImage, ecode);

    /* Mono sampled image parameters */

    ecode = psdf_put_image_params(pdev, plist, &Mono_names, Mono_filters,
				  &params.MonoImage, ecode);

    /* Font embedding parameters */

    ecode = psdf_put_embed_param(plist, "~AlwaysEmbed",
				 &params.AlwaysEmbed, ecode);
    ecode = psdf_put_embed_param(plist, "~NeverEmbed",
				 &params.NeverEmbed, ecode);
    ecode = psdf_put_bool_param(plist, "EmbedAllFonts",
				&params.EmbedAllFonts, ecode);
    ecode = psdf_put_bool_param(plist, "SubsetFonts",
				&params.SubsetFonts, ecode);
    ecode = psdf_put_int_param(plist, "MaxSubsetPct",
			       &params.MaxSubsetPct, ecode);

    if (ecode < 0)
	return ecode;
    code = gdev_vector_put_params(dev, plist);
    if (code < 0)
	return code;

    pdev->params = params;	/* OK to update now */
    return 0;
}
