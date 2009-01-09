/*
 * "$Id$"
 *
 *   PWG media name API implementation for the Common UNIX Printing System
 *   (CUPS).
 *
 *   Copyright 2009 by Apple Inc.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   _cupsPWGMediaByLegacy() - Find a PWG media size by ISO/IPP legacy name.
 *   _cupsPWGMediaByName()   - Find a PWG media size by 5101.1 self-describing
 *                             name.
 *   _cupsPWGMediaBySize()   - Find a PWG media size by size in points.
 *   compare_legacy()        - Compare two sizes using the legacy names.
 *   compare_pwg()           - Compare two sizes using the PWG names.
 */

/*
 * Include necessary headers...
 */

#include "pwgmedia.h"
#include "globals.h"
#include "string.h"
#include "debug.h"
#include <math.h>


/*
 * Local macros...
 */

#define _CUPS_SIZE_IN(p,l,x,y)	{p, l, x * 72.0, y * 72.0}
#define _CUPS_SIZE_MM(p,l,x,y)	{p, l, x / 25.4 * 72.0, y / 25.4 * 72.0}


/*
 * Local functions...
 */

static int	compare_legacy(_cups_pwg_media_t *a, _cups_pwg_media_t *b);
static int	compare_pwg(_cups_pwg_media_t *a, _cups_pwg_media_t *b);


/*
 * Local globals...
 */

static _cups_pwg_media_t const cups_pwg_media[] =
{					/* Media size lookup table */
  /* North American Standard Sheet Media Sizes */
  _CUPS_SIZE_IN("na_index-3x5_3x5in", NULL, 3, 5),
  _CUPS_SIZE_IN("na_personal_3.625x6.5in", NULL, 3.625, 6.5),
  _CUPS_SIZE_IN("na_monarch_3.875x7.5in", "monarch-envelope", 3.875, 7.5),
  _CUPS_SIZE_IN("na_number-9_3.875x8.875in", "na-number-9-envelope", 3.875, 8.875),
  _CUPS_SIZE_IN("na_index-4x6_4x6in", NULL, 4, 6),
  _CUPS_SIZE_IN("na_number-10_4.125x9.5in", "na-number-10-envelope", 4.125, 9.5),
  _CUPS_SIZE_IN("na_a2_4.375x5.75in", NULL, 4.375, 5.75),
  _CUPS_SIZE_IN("na_number-11_4.5x10.375in", NULL, 4.5, 10.375),
  _CUPS_SIZE_IN("na_number-12_4.75x11in", NULL, 4.75, 11),
  _CUPS_SIZE_IN("na_5x7_5x7in", NULL, 5, 7),
  _CUPS_SIZE_IN("na_index-5x8_5x8in", NULL, 5, 8),
  _CUPS_SIZE_IN("na_number-14_5x11.5in", NULL, 5, 11.5),
  _CUPS_SIZE_IN("na_invoice_5.5x8.5in", "invoice", 5.5, 8.5),
  _CUPS_SIZE_IN("na_index-4x6-ext_6x8in", NULL, 6, 8),
  _CUPS_SIZE_IN("na_6x9_6x9in", "na-6x9-envelope", 6, 9),
  _CUPS_SIZE_IN("na_c5_6.5x9.5in", NULL, 6.5, 9.5),
  _CUPS_SIZE_IN("na_7x9_7x9in", "na-7x9-envelope", 7, 9),
  _CUPS_SIZE_IN("na_executive_7.25x10.5in", "executive", 7.25, 10.5),
  _CUPS_SIZE_IN("na_govt-letter_8x10in", "na-8x10", 8, 10),
  _CUPS_SIZE_IN("na_govt-legal_8x13in", NULL, 8, 13),
  _CUPS_SIZE_IN("na_quarto_8.5x10.83in", "quarto", 8.5, 10.83),
  _CUPS_SIZE_IN("na_letter_8.5x11in", "na-letter", 8.5, 11),
  _CUPS_SIZE_IN("na_fanfold-eur_8.5x12in", NULL, 8.5, 12),
  _CUPS_SIZE_IN("na_letter-plus_8.5x12.69in", NULL, 8.5, 12.69),
  _CUPS_SIZE_IN("na_foolscap_8.5x13in", NULL, 8.5, 13),
  _CUPS_SIZE_IN("na_legal_8.5x14in", "na-legal", 8.5, 14),
  _CUPS_SIZE_IN("na_super-a_8.94x14in", NULL, 8.94, 14),
  _CUPS_SIZE_IN("na_9x11_9x11in", "na-9x11-envelope", 9, 11),
  _CUPS_SIZE_IN("na_arch-a_9x12in", "arch-a", 9, 12),
  _CUPS_SIZE_IN("na_letter-extra_9.5x12in", NULL, 9.5, 12),
  _CUPS_SIZE_IN("na_legal-extra_9.5x15in", NULL, 9.5, 15),
  _CUPS_SIZE_IN("na_10x11_10x11in", NULL, 10, 11),
  _CUPS_SIZE_IN("na_10x13_10x13in", "na-10x13-envelope", 10, 13),
  _CUPS_SIZE_IN("na_10x14_10x14in", "na-10x14-envelope", 10, 14),
  _CUPS_SIZE_IN("na_10x15_10x15in", "na-10x15-envelope", 10, 15),
  _CUPS_SIZE_IN("na_11x12_11x12in", NULL, 11, 12),
  _CUPS_SIZE_IN("na_edp_11x14in", NULL, 11, 14),
  _CUPS_SIZE_IN("na_fanfold-us_11x14.875in", NULL, 11, 14.875),
  _CUPS_SIZE_IN("na_11x15_11x15in", NULL, 11, 15),
  _CUPS_SIZE_IN("na_ledger_11x17in", "tabloid", 11, 17),
  _CUPS_SIZE_IN("na_eur-edp_12x14in", NULL, 12, 14),
  _CUPS_SIZE_IN("na_arch-b_12x18in", "arch-b", 12, 18),
  _CUPS_SIZE_IN("na_12x19_12x19in", NULL, 12, 19),
  _CUPS_SIZE_IN("na_b-plus_12x19.17in", NULL, 12, 19.17),
  _CUPS_SIZE_IN("na_super-b_13x19in", NULL, 13, 19),
  _CUPS_SIZE_IN("na_c_17x22in", "c", 17, 22),
  _CUPS_SIZE_IN("na_arch-c_18x24in", "arch-c", 18, 24),
  _CUPS_SIZE_IN("na_d_22x34in", "d", 22, 34),
  _CUPS_SIZE_IN("na_arch-d_24x36in", "arch-d", 24, 36),
  _CUPS_SIZE_IN("asme_f_28x40in", "f", 28, 40),
  _CUPS_SIZE_IN("na_wide-format_30x42in", NULL, 30, 42),
  _CUPS_SIZE_IN("na_e_34x44in", "e", 34, 44),
  _CUPS_SIZE_IN("na_arch-e_36x48in", "arch-e", 36, 48),
  _CUPS_SIZE_IN("na_f_44x68in", NULL, 44, 68),

  /* Chinese Standard Sheet Media Inch Sizes */
  _CUPS_SIZE_IN("roc_16k_7.75x10.75in", NULL, 7.75, 10.75),
  _CUPS_SIZE_IN("roc_8k_10.75x15.5in", NULL, 10.75, 15.5),

  /* ISO Standard Sheet Media Sizes */
  _CUPS_SIZE_MM("iso_a10_26x37mm", "iso-a10", 26, 37),
  _CUPS_SIZE_MM("iso_a9_37x52mm", "iso-a9", 37, 52),
  _CUPS_SIZE_MM("iso_a8_52x74mm", "iso-a8", 52, 74),
  _CUPS_SIZE_MM("iso_a7_74x105mm", "iso-a7", 74, 105),
  _CUPS_SIZE_MM("iso_a6_105x148mm", "iso-a6", 105, 148),
  _CUPS_SIZE_MM("iso_a5_148x210mm", "iso-a5", 148, 210),
  _CUPS_SIZE_MM("iso_a5-extra_174x235mm", NULL, 174, 235),
  _CUPS_SIZE_MM("iso_a4_210x297mm", "iso-a4", 210, 297),
  _CUPS_SIZE_MM("iso_a4-tab_225x297mm", NULL, 225, 297),
  _CUPS_SIZE_MM("iso_a4-extra_235.5x322.3mm", NULL, 235.5, 322.3),
  _CUPS_SIZE_MM("iso_a3_297x420mm", "iso-a3", 297, 420),
  _CUPS_SIZE_MM("iso_a4x3_297x630mm", "iso-a4x3", 297, 630),
  _CUPS_SIZE_MM("iso_a4x4_297x841mm", "iso-a4x4", 297, 841),
  _CUPS_SIZE_MM("iso_a4x5_297x1051mm", "iso-a4x5", 297, 1051),
  _CUPS_SIZE_MM("iso_a4x6_297x1261mm", "iso-a4x6", 297, 1261),
  _CUPS_SIZE_MM("iso_a4x7_297x1471mm", "iso-a4x7", 297, 1471),
  _CUPS_SIZE_MM("iso_a4x8_297x1682mm", "iso-a4x8", 297, 1682),
  _CUPS_SIZE_MM("iso_a4x9_297x1892mm", "iso-a4x9", 297, 1892),
  _CUPS_SIZE_MM("iso_a3-extra_322x445mm", "iso-a3-extra", 322, 445),
  _CUPS_SIZE_MM("iso_a2_420x594mm", "iso-a2", 420, 594),
  _CUPS_SIZE_MM("iso_a3x3_420x891mm", "iso-a3x3", 420, 891),
  _CUPS_SIZE_MM("iso_a3x4_420x1189mm", "iso-a3x4", 420, 1189),
  _CUPS_SIZE_MM("iso_a3x5_420x1486mm", "iso-a3x5", 420, 1486),
  _CUPS_SIZE_MM("iso_a3x6_420x1783mm", "iso-a3x6", 420, 1783),
  _CUPS_SIZE_MM("iso_a3x7_420x2080mm", "iso-a3x7", 420, 2080),
  _CUPS_SIZE_MM("iso_a1_594x841mm", "iso-a1", 594, 841),
  _CUPS_SIZE_MM("iso_a2x3_594x1261mm", "iso-a2x3", 594, 1261),
  _CUPS_SIZE_MM("iso_a2x4_594x1682mm", "iso-a2x4", 594, 1682),
  _CUPS_SIZE_MM("iso_a2x5_594x2102mm", "iso-a2x5", 594, 2102),
  _CUPS_SIZE_MM("iso_a0_841x1189mm", "iso-a0", 841, 1189),
  _CUPS_SIZE_MM("iso_a1x3_841x1783mm", "iso-a1x3", 841, 1783),
  _CUPS_SIZE_MM("iso_a1x4_841x2378mm", "iso-a1x4", 841, 2378),
  _CUPS_SIZE_MM("iso_2a0_1189x1682mm", NULL, 1189, 1682),
  _CUPS_SIZE_MM("iso_a0x3_1189x2523mm", NULL, 1189, 2523),
  _CUPS_SIZE_MM("iso_b10_31x44mm", "iso-b10", 31, 44),
  _CUPS_SIZE_MM("iso_b9_44x62mm", "iso-b9", 44, 62),
  _CUPS_SIZE_MM("iso_b8_62x88mm", "iso-b8", 62, 88),
  _CUPS_SIZE_MM("iso_b7_88x125mm", "iso-b7", 88, 125),
  _CUPS_SIZE_MM("iso_b6_125x176mm", "iso-b6", 125, 176),
  _CUPS_SIZE_MM("iso_b6c4_125x324mm", NULL, 125, 324),
  _CUPS_SIZE_MM("iso_b5_176x250mm", "iso-b5", 176, 250),
  _CUPS_SIZE_MM("iso_b5-extra_201x276mm", NULL, 201, 276),
  _CUPS_SIZE_MM("iso_b4_250x353mm", "iso-b4", 250, 353),
  _CUPS_SIZE_MM("iso_b3_353x500mm", "iso-b3", 353, 500),
  _CUPS_SIZE_MM("iso_b2_500x707mm", "iso-b2", 500, 707),
  _CUPS_SIZE_MM("iso_b1_707x1000mm", "iso-b1", 707, 1000),
  _CUPS_SIZE_MM("iso_b0_1000x1414mm", "iso-b0", 1000, 1414),
  _CUPS_SIZE_MM("iso_c10_28x40mm", "iso-c10", 28, 40),
  _CUPS_SIZE_MM("iso_c9_40x57mm", "iso-c9", 40, 57),
  _CUPS_SIZE_MM("iso_c8_57x81mm", "iso-c8", 57, 81),
  _CUPS_SIZE_MM("iso_c7_81x114mm", "iso-c7", 81, 114),
  _CUPS_SIZE_MM("iso_c7c6_81x162mm", NULL, 81, 162),
  _CUPS_SIZE_MM("iso_c6_114x162mm", "iso-c6", 114, 162),
  _CUPS_SIZE_MM("iso_c6c5_114x229mm", NULL, 114, 229),
  _CUPS_SIZE_MM("iso_c5_162x229mm", "iso-c5", 162, 229),
  _CUPS_SIZE_MM("iso_c4_229x324mm", "iso-c4", 229, 324),
  _CUPS_SIZE_MM("iso_c3_324x458mm", "iso-c3", 324, 458),
  _CUPS_SIZE_MM("iso_c2_458x648mm", "iso-c2", 458, 648),
  _CUPS_SIZE_MM("iso_c1_648x917mm", "iso-c1", 648, 917),
  _CUPS_SIZE_MM("iso_c0_917x1297mm", "iso-c0", 917, 1297),
  _CUPS_SIZE_MM("iso_dl_110x220mm", "iso-designated", 110, 220),
  _CUPS_SIZE_MM("iso_ra2_430x610mm", "iso-ra2", 430, 610),
  _CUPS_SIZE_MM("iso_sra2_450x640mm", "iso-sra2", 450, 640),
  _CUPS_SIZE_MM("iso_ra1_610x860mm", "iso-ra1", 610, 860),
  _CUPS_SIZE_MM("iso_sra1_640x900mm", "iso-sra1", 640, 900),
  _CUPS_SIZE_MM("iso_ra0_860x1220mm", "iso-ra0", 860, 1220),
  _CUPS_SIZE_MM("iso_sra0_900x1280mm", "iso-sra0", 900, 1280),

  /* Japanese Standard Sheet Media Sizes */
  _CUPS_SIZE_MM("jis_b10_32x45mm", "jis-b10", 32, 45),
  _CUPS_SIZE_MM("jis_b9_45x64mm", "jis-b9", 45, 64),
  _CUPS_SIZE_MM("jis_b8_64x91mm", "jis-b8", 64, 91),
  _CUPS_SIZE_MM("jis_b7_91x128mm", "jis-b7", 91, 128),
  _CUPS_SIZE_MM("jis_b6_128x182mm", "jis-b6", 128, 182),
  _CUPS_SIZE_MM("jis_b5_182x257mm", "jis-b5", 182, 257),
  _CUPS_SIZE_MM("jis_b4_257x364mm", "jis-b4", 257, 364),
  _CUPS_SIZE_MM("jis_b3_364x515mm", "jis-b3", 364, 515),
  _CUPS_SIZE_MM("jis_b2_515x728mm", "jis-b2", 515, 728),
  _CUPS_SIZE_MM("jis_b1_728x1030mm", "jis-b1", 728, 1030),
  _CUPS_SIZE_MM("jis_b0_1030x1456mm", "jis-b0", 1030, 1456),
  _CUPS_SIZE_MM("jis_exec_216x330mm", NULL, 216, 330),
  _CUPS_SIZE_MM("jpn_chou4_90x205mm", NULL, 90, 205),
  _CUPS_SIZE_MM("jpn_hagaki_100x148mm", NULL, 100, 148),
  _CUPS_SIZE_MM("jpn_you4_105x235mm", NULL, 105, 235),
  _CUPS_SIZE_MM("jpn_chou2_111.1x146mm", NULL, 111.1, 146),
  _CUPS_SIZE_MM("jpn_chou3_120x235mm", NULL, 120, 235),
  _CUPS_SIZE_MM("jpn_oufuku_148x200mm", NULL, 148, 200),
  _CUPS_SIZE_MM("jpn_kahu_240x322.1mm", NULL, 240, 322.1),
  _CUPS_SIZE_MM("jpn_kaku2_240x332mm", NULL, 240, 332),

  /* Chinese Standard Sheet Media Sizes */
  _CUPS_SIZE_MM("prc_32k_97x151mm", NULL, 97, 151),
  _CUPS_SIZE_MM("prc_1_102x165mm", NULL, 102, 165),
  _CUPS_SIZE_MM("prc_2_102x176mm", NULL, 102, 176),
  _CUPS_SIZE_MM("prc_4_110x208mm", NULL, 110, 208),
  _CUPS_SIZE_MM("prc_5_110x220mm", NULL, 110, 220),
  _CUPS_SIZE_MM("prc_8_120x309mm", NULL, 120, 309),
  _CUPS_SIZE_MM("prc_6_120x320mm", NULL, 120, 320),
  _CUPS_SIZE_MM("prc_3_125x176mm", NULL, 125, 176),
  _CUPS_SIZE_MM("prc_16k_146x215mm", NULL, 146, 215),
  _CUPS_SIZE_MM("prc_7_160x230mm", NULL, 160, 230),
  _CUPS_SIZE_MM("om_juuro-ku-kai_198x275mm", NULL, 198, 275),
  _CUPS_SIZE_MM("om_pa-kai_267x389mm", NULL, 267, 389),
  _CUPS_SIZE_MM("om_dai-pa-kai_275x395mm", NULL, 275, 395),
  _CUPS_SIZE_MM("prc_10_324x458mm", NULL, 324, 458),

  /* Other Metric Standard Sheet Media Sizes */
  _CUPS_SIZE_MM("om_small-photo_100x150mm", NULL, 100, 150),
  _CUPS_SIZE_MM("om_italian_110x230mm", NULL, 110, 230),
  _CUPS_SIZE_MM("om_postfix_114x229mm", NULL, 114, 229),
  _CUPS_SIZE_MM("om_large-photo_200x300", NULL, 200, 300),
  _CUPS_SIZE_MM("om_folio_210x330mm", "folio", 210, 330),
  _CUPS_SIZE_MM("om_folio-sp_215x315mm", NULL, 215, 315),
  _CUPS_SIZE_MM("om_invite_220x220mm", NULL, 220, 220)
};


/*
 * '_cupsPWGMediaByLegacy()' - Find a PWG media size by ISO/IPP legacy name.
 */

_cups_pwg_media_t *			/* O - Matching size or NULL */
_cupsPWGMediaByLegacy(
    const char *legacy)			/* I - Legacy size name */
{
  _cups_pwg_media_t	key;		/* Search key */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * Build the lookup table for PWG names as needed...
  */

  if (!cg->leg_size_lut)
  {
    int			i;		/* Looping var */
    _cups_pwg_media_t	*size;		/* Current size */

    cg->leg_size_lut = cupsArrayNew((cups_array_func_t)compare_legacy, NULL);

    for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
             size = (_cups_pwg_media_t *)cups_pwg_media;
	 i > 0;
	 i --, size ++)
      if (size->legacy)
	cupsArrayAdd(cg->leg_size_lut, size);
  }

 /*
  * Lookup the name...
  */

  key.legacy = legacy;
  return ((_cups_pwg_media_t *)cupsArrayFind(cg->leg_size_lut, &key));
}


/*
 * '_cupsPWGMediaByName()' - Find a PWG media size by 5101.1 self-describing
 *                           name.
 */

_cups_pwg_media_t *			/* O - Matching size or NULL */
_cupsPWGMediaByName(const char *pwg)	/* I - PWG size name */
{
  _cups_pwg_media_t	key;		/* Search key */
  _cups_globals_t	*cg = _cupsGlobals();
					/* Global data */


 /*
  * Build the lookup table for PWG names as needed...
  */

  if (!cg->pwg_size_lut)
  {
    int			i;		/* Looping var */
    _cups_pwg_media_t	*size;		/* Current size */

    cg->pwg_size_lut = cupsArrayNew((cups_array_func_t)compare_pwg, NULL);

    for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
             size = (_cups_pwg_media_t *)cups_pwg_media;
	 i > 0;
	 i --, size ++)
      cupsArrayAdd(cg->pwg_size_lut, size);
  }

 /*
  * Lookup the name...
  */

  key.pwg = pwg;
  return ((_cups_pwg_media_t *)cupsArrayFind(cg->pwg_size_lut, &key));
}


/*
 * '_cupsPWGMediaBySize()' - Find a PWG media size by size in points.
 */

_cups_pwg_media_t *			/* O - Matching size or NULL */
_cupsPWGMediaBySize(double width,	/* I - Width in points */
                    double length)	/* I - Length in points */
{
  int			i;		/* Looping var */
  _cups_pwg_media_t	*size;		/* Current size */
  double		dw, dl;		/* Difference in width and length */


  for (i = (int)(sizeof(cups_pwg_media) / sizeof(cups_pwg_media[0])),
	   size = (_cups_pwg_media_t *)cups_pwg_media;
       i > 0;
       i --, size ++)
  {
   /*
    * Adobe uses a size matching algorithm with an epsilon of 5 points...
    */

    dw = size->width - width;
    dl = size->length - length;

    if (dw > -5.0 && dw < 5.0 && dl > -5.0 && dl < 5.0)
      return (size);
  }

  return (NULL);
}


/*
 * 'compare_legacy()' - Compare two sizes using the legacy names.
 */

static int				/* O - Result of comparison */
compare_legacy(_cups_pwg_media_t *a,	/* I - First size */
               _cups_pwg_media_t *b)	/* I - Second size */
{
  return (strcmp(a->legacy, b->legacy));
}


/*
 * 'compare_pwg()' - Compare two sizes using the PWG names.
 */

static int				/* O - Result of comparison */
compare_pwg(_cups_pwg_media_t *a,	/* I - First size */
            _cups_pwg_media_t *b)	/* I - Second size */
{
  return (strcmp(a->pwg, b->pwg));
}


/*
 * End of "$Id$".
 */
