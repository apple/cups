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

/* gdevpdfm.c */
/* Get/put parameters for PDF-writing driver */
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"
#include "strimpl.h"		/* for short-sighted compilers */
#include "scfx.h"
#include "slzwx.h"
#include "srlx.h"

/*
 * The pdfwrite device supports the following "real" parameters:
 *	OutputFile <string>
 *	(all the Distiller parameters except *ImageDict)
 * Currently, the only Distiller parameter that actually has any effect
 * is ASCII85EncodePages.
 *
 * The device also supports the following write-only pseudo-parameters that
 * serve only to communicate other information from the PostScript file.
 * Their "value" is an array of strings, some of which may be the result
 * of converting arbitrary PostScript objects to string form.
 *	pdfmark - see gdevpdfm.c
 *	show - see gdevpdft.c
 */

private const int CoreDistVersion = 2000;	/* Distiller 2.0 */

typedef struct pdf_image_filter_name_s {
  const char *pname;
  const stream_template *template;
} pdf_image_filter_name;
private const char *Color_names[6] = {
  "DownsampleColorImages", "ColorImageResolution", "EncodeColorImages",
  "ColorImageFilter", "ColorImageDepth", "AntiAliasColorImages"
};
private const pdf_image_filter_name Poly_filters[] = {
  /*{"DCTEncode", &s_DCTE_template},*/
  {"LZWEncode", &s_LZWE_template},
  {0, 0}
};
private const char *Gray_names[6] = {
  "DownsampleGrayImages", "GrayImageResolution", "EncodeGrayImages",
  "GrayImageFilter", "GrayImageDepth", "AntiAliasGrayImages"
};
private const char *Mono_names[6] = {
  "DownsampleMonoImages", "MonoImageResolution", "EncodeMonoImages",
  "MonoImageFilter", "MonoImageDepth", "AntiAliasMonoImages"
};
private const pdf_image_filter_name Mono_filters[] = {
  {"CCITTFaxEncode", &s_CFE_template},
  {"LZWEncode", &s_LZWE_template},
  {"RunLengthEncode", &s_RLE_template},
  {0, 0}
};

/* ---------------- Get parameters ---------------- */

/* Get a set of image-related parameters. */
private int
pdf_get_image_params(gs_param_list *plist, const char *pnames[6],
  pdf_image_params *params)
{	int code;
	gs_param_string fstr;

	if ( (code = param_write_bool(plist, pnames[0], &params->Downsample)) < 0 ||
	     (code = param_write_int(plist, pnames[1], &params->Resolution)) < 0 ||
	     (code = param_write_bool(plist, pnames[2], &params->Encode)) < 0 ||
	     (code = (params->Filter == 0 ? 0 :
		      (param_string_from_string(fstr, params->Filter),
		       param_write_name(plist, pnames[3], &fstr)))) < 0 ||
	     /*Dict*/
	     (code = param_write_int(plist, pnames[4], &params->Depth)) < 0 ||
	     (code = param_write_bool(plist, pnames[5], &params->AntiAlias)) < 0
	   )
	  return code;
	return code;
}

/* Get parameters. */
int
gdev_pdf_get_params(gx_device *dev, gs_param_list *plist)
{	gx_device_pdf *pdev = (gx_device_pdf *)dev;
	int code = gx_default_get_params(dev, plist);
	gs_param_string ofns;

	if ( code < 0 )
	  return code;
	ofns.data = (const byte *)pdev->fname,
	  ofns.size = strlen(pdev->fname),
	  ofns.persistent = false;
	if ( (code = param_write_int(plist, "CoreDistVersion", (int *)&CoreDistVersion)) < 0 ||
	     (code = param_write_string(plist, "OutputFile", &ofns)) < 0 ||
	     (code = param_write_bool(plist, "DoThumbnails", &pdev->params.DoThumbnails)) < 0 ||
	     (code = param_write_bool(plist, "LZWEncodePages", &pdev->params.LZWEncodePages)) < 0 ||
	     (code = param_write_bool(plist, "ASCII85EncodePages", &pdev->params.ASCII85EncodePages)) < 0 ||
	     (code = pdf_get_image_params(plist, Color_names, &pdev->params.ColorImage)) < 0 ||
	     /*ColorImageDict*/
	     (code = param_write_bool(plist, "ConvertCMYKImagesToRGB", &pdev->params.ConvertCMYKImagesToRGB)) < 0 ||
	     (code = pdf_get_image_params(plist, Gray_names, &pdev->params.GrayImage)) < 0 ||
	     /*GrayImageDict*/
	     (code = pdf_get_image_params(plist, Mono_names, &pdev->params.MonoImage)) < 0 ||
	     /*MonoImageDict*/
	     (code = param_write_name_array(plist, "AlwaysEmbed", &pdev->params.AlwaysEmbed)) < 0 ||
	     (code = param_write_name_array(plist, "NeverEmbed", &pdev->params.NeverEmbed)) < 0 ||
	     (code = param_write_bool(plist, "EmbedAllFonts", &pdev->params.EmbedAllFonts)) < 0 ||
	     (code = param_write_bool(plist, "SubsetFonts", &pdev->params.SubsetFonts)) < 0 ||
	     (code = param_write_int(plist, "MaxSubsetPct", &pdev->params.MaxSubsetPct)) < 0
	   )
	  return code;
	return code;
}

/* ---------------- Put parameters ---------------- */

/* Compare a C string and a gs_param_string. */
bool
pdf_key_eq(const gs_param_string *pcs, const char *str)
{	return (strlen(str) == pcs->size &&
		!strncmp(str, (const char *)pcs->data, pcs->size));
}

/* Put a Boolean or integer parameter. */
private int
pdf_put_bool_param(gs_param_list *plist, gs_param_name param_name,
  bool *pval, int ecode)
{	int code;
	switch ( code = param_read_bool(plist, param_name, pval) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}
	return ecode;
}
private int
pdf_put_int_param(gs_param_list *plist, gs_param_name param_name,
  int *pval, int ecode)
{	int code;
	switch ( code = param_read_int(plist, param_name, pval) )
	{
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
pdf_put_embed_param(gs_param_list *plist, gs_param_name notpname,
  gs_param_string_array *psa, int ecode)
{	gs_param_name pname = notpname + 1;
	int code;
	gs_param_string_array nsa;

	/***** Storage management is incomplete ******/
	/***** Doesn't do incremental add/delete ******/
	switch ( code = param_read_name_array(plist, pname, psa) )
	  {
	  default:
		ecode = code;
		param_signal_error(plist, pname, ecode);
	  case 0:
	  case 1:
		break;
	  }
	switch ( code = param_read_name_array(plist, notpname, &nsa) )
	  {
	  default:
		ecode = code;
		param_signal_error(plist, notpname, ecode);
	  case 0:
	  case 1:
		break;
	  }
	return ecode;
}

/* Put a set of image-related parameters. */
private int
pdf_put_image_params(gs_param_list *plist, const char *pnames[6],
  const pdf_image_filter_name *pifn, pdf_image_params *params, int ecode)
{	gs_param_string fs;
	int code;

	ecode = pdf_put_bool_param(plist, pnames[0], &params->Downsample,
				   ecode);
	ecode = pdf_put_int_param(plist, pnames[1], &params->Resolution,
				  ecode);
	ecode = pdf_put_bool_param(plist, pnames[2], &params->Encode,
				   ecode);
	switch ( code = param_read_string(plist, pnames[3], &fs) )
	  {
	  case 0:
	    {	const pdf_image_filter_name *pn = pifn;
		while ( pn->pname != 0 && !pdf_key_eq(&fs, pn->pname) )
		  pn++;
		if ( pn->pname == 0 )
		  { ecode = gs_error_rangecheck;
		    goto ipe;
		  }
		params->Filter = pn->pname;
		params->filter_template = pn->template;
		break;
	    }
	  default:
		ecode = code;
ipe:		param_signal_error(plist, pnames[3], ecode);
	  case 1:
		break;
	  }
	/*Dict*/
	ecode = pdf_put_int_param(plist, pnames[4], &params->Depth,
				  ecode);
	ecode = pdf_put_bool_param(plist, pnames[5], &params->AntiAlias,
				   ecode);
	if ( ecode >= 0 )
	  { /* Force parameters to acceptable values. */
	    if ( params->Resolution < 1 )
	      params->Resolution = 1;
	    switch ( params->Depth )
	      {
	      default:
		params->Depth = -1;
	      case 1: case 2: case 4: case 8:
	      case -1:
		break;
	      }
	  }
	return ecode;
}

/* Put parameters. */
int
gdev_pdf_put_params(gx_device *dev, gs_param_list *plist)
{	gx_device_pdf *pdev = (gx_device_pdf *)dev;
	int ecode = 0;
	int code;
	gs_param_name param_name;
	gs_param_string ofs;
	pdf_distiller_params params;

	/* Handle Distiller parameters. */

	{ int cdv = CoreDistVersion;
	  ecode = pdf_put_int_param(plist, (param_name = "CoreDistVersion"), &cdv, ecode);
	  if ( cdv != CoreDistVersion )
	    param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
	}

	params = pdev->params;
	switch ( code = param_read_string(plist, (param_name = "OutputFile"), &ofs) )
	{
	case 0:
		if ( ofs.size > fname_size )
		  ecode = gs_error_limitcheck;
		else
		  break;
		goto ofe;
	default:
		ecode = code;
ofe:		param_signal_error(plist, param_name, ecode);
	case 1:
		ofs.data = 0;
		break;
	}

	ecode = pdf_put_bool_param(plist, "DoThumbnails",
				   &params.DoThumbnails, ecode);
	ecode = pdf_put_bool_param(plist, "LZWEncodePages",
				   &params.LZWEncodePages, ecode);
	ecode = pdf_put_bool_param(plist, "ASCII85EncodePages",
				   &params.ASCII85EncodePages, ecode);
	ecode = pdf_put_image_params(plist, Color_names, Poly_filters,
				     &params.ColorImage, ecode);
	/*ColorImageDict*/
	ecode = pdf_put_bool_param(plist, "ConvertCMYKImagesToRGB",
				   &params.ConvertCMYKImagesToRGB, ecode);
	ecode = pdf_put_image_params(plist, Gray_names, Poly_filters,
				     &params.GrayImage, ecode);
	/*GrayImageDict*/
	ecode = pdf_put_image_params(plist, Mono_names, Mono_filters,
				     &params.MonoImage, ecode);
	/*MonoImageDict*/
	ecode = pdf_put_embed_param(plist, "~AlwaysEmbed",
				    &params.AlwaysEmbed, ecode);
	ecode = pdf_put_embed_param(plist, "~NeverEmbed",
				    &params.NeverEmbed, ecode);
	ecode = pdf_put_bool_param(plist, "EmbedAllFonts",
				   &params.EmbedAllFonts, ecode);
	ecode = pdf_put_bool_param(plist, "SubsetFonts",
				   &params.SubsetFonts, ecode);
	ecode = pdf_put_int_param(plist, "MaxSubsetPct",
				  &params.MaxSubsetPct, ecode);

	/* Handle pseudo-parameters. */

	{ gs_param_string_array ppa;
	  switch ( code = param_read_string_array(plist,
						  (param_name = "pdfmark"),
						  &ppa) )
	  {
	  case 0:
	    pdf_open_document(pdev);
	    code = pdfmark_process(pdev, &ppa);
	    if ( code >= 0 )
	      break;
	    /* falls through for errors */
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	  case 1:
	    break;
	  }
	}

	{ gs_param_dict ppd;
	  switch ( code = param_begin_read_dict(plist,
						(param_name = "show"),
						&ppd, false) )
	  {
	  case 0:
	    pdf_open_document(pdev);
	    code = pdfshow_process(pdev, &ppd);
	    param_end_read_dict(plist, param_name, &ppd);
	    if ( code >= 0 )
	      break;
	    /* falls through for errors */
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	  case 1:
	    break;
	  }
	}

	if ( ecode < 0 )
	  return ecode;
	{ bool open = dev->is_open;
	  /* Don't let gx_default_put_params close the device. */
	  dev->is_open = false;
	  code = gx_default_put_params(dev, plist);
	  dev->is_open = open;
	}
	if ( code < 0 )
	  return code;

	if ( ofs.data != 0 &&
	     bytes_compare(ofs.data, ofs.size,
			   (const byte *)pdev->fname, strlen(pdev->fname))
	   )
	  {	/* Close the file if it's open. */
		if ( pdev->file != 0 )
		  {	fclose(pdev->file);
			pdev->file = 0;
		  }
		memcpy(pdev->fname, ofs.data, ofs.size);
		pdev->fname[ofs.size] = 0;
		if ( dev->is_open )
		{	/* Reopen the file now. */
			pdev->file = gp_fopen(pdev->fname, gp_fmode_wb);
			if ( pdev->file == 0 )
			  return_error(gs_error_ioerror);
		}
	  }
	pdev->params = params;		/* OK to update now */
	pdf_set_scale(pdev);

	return 0;
}
