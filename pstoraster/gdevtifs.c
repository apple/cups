/* Copyright (C) 1994, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevtifs.c */
/* TIFF-writing substructure */
#include "stdio_.h"
#include "time_.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"
#include "gdevtifs.h"

/*
 * Define the standard contents of a TIFF directory.
 * Clients may add more items, also sorted in increasing tag order.
 */
typedef struct TIFF_std_directory_entries_s {
	TIFF_dir_entry	SubFileType;
	TIFF_dir_entry	ImageWidth;
	TIFF_dir_entry	ImageLength;
	TIFF_dir_entry	StripOffsets;
	TIFF_dir_entry	Orientation;
	TIFF_dir_entry	RowsPerStrip;
	TIFF_dir_entry	StripByteCounts;
	TIFF_dir_entry	XResolution;
	TIFF_dir_entry	YResolution;
	TIFF_dir_entry	PlanarConfig;
	TIFF_dir_entry	ResolutionUnit;
	TIFF_dir_entry	PageNumber;
	TIFF_dir_entry	Software;
	TIFF_dir_entry	DateTime;
} TIFF_std_directory_entries;
/* Define values that follow the directory entries. */
typedef struct TIFF_std_directory_values_s {
	TIFF_ulong	diroff;			/* offset to next directory */
	TIFF_ulong	xresValue[2];		/* XResolution indirect value */
	TIFF_ulong	yresValue[2];		/* YResolution indirect value */
#define maxSoftware 40
	char		softwareValue[maxSoftware]; /* Software indirect value */
	char		dateTimeValue[20];	/* DateTime indirect value */
} TIFF_std_directory_values;
private const TIFF_std_directory_entries std_entries_initial = {
	{ TIFFTAG_SubFileType,	TIFF_LONG,  1, SubFileType_page },
	{ TIFFTAG_ImageWidth,	TIFF_LONG,  1 },
	{ TIFFTAG_ImageLength,	TIFF_LONG,  1 },
	{ TIFFTAG_StripOffsets,	TIFF_LONG,  1 },
	{ TIFFTAG_Orientation,	TIFF_SHORT, 1, Orientation_top_left },
	{ TIFFTAG_RowsPerStrip,	TIFF_LONG,  1 },
	{ TIFFTAG_StripByteCounts, TIFF_LONG,  1 },
	{ TIFFTAG_XResolution,	TIFF_RATIONAL | TIFF_INDIRECT, 1,
	    offset_of(TIFF_std_directory_values, xresValue[0]) },
	{ TIFFTAG_YResolution,	TIFF_RATIONAL | TIFF_INDIRECT, 1,
	    offset_of(TIFF_std_directory_values, yresValue[0]) },
	{ TIFFTAG_PlanarConfig,	TIFF_SHORT, 1, PlanarConfig_contig },
	{ TIFFTAG_ResolutionUnit, TIFF_SHORT, 1, ResolutionUnit_inch },
	{ TIFFTAG_PageNumber,	TIFF_SHORT, 2 },
	{ TIFFTAG_Software,	TIFF_ASCII | TIFF_INDIRECT, 0,
	    offset_of(TIFF_std_directory_values, softwareValue[0]) },
	{ TIFFTAG_DateTime,	TIFF_ASCII | TIFF_INDIRECT, 20,
	    offset_of(TIFF_std_directory_values, dateTimeValue[0]) }
};
private const TIFF_std_directory_values std_values_initial = {
	0, { 0, 1 }, { 0, 1 }, { 0 }, { 0, 0 }
};

/* Fix up tag values on big-endian machines if necessary. */
#if arch_is_big_endian
private void
tiff_fixup_tag(TIFF_dir_entry *dp)
{	switch ( dp->type )
	  {
	  case TIFF_SHORT: case TIFF_SSHORT:
	    /* We may have two shorts packed into a TIFF_ulong. */
	    dp->value = (dp->value << 16) + (dp->value >> 16); break;
	  case TIFF_BYTE: case TIFF_SBYTE:
	    dp->value <<= 24; break;
	  }
}
#else
#  define tiff_fixup_tag(dp) DO_NOTHING
#endif

/* Begin a TIFF page. */
int
gdev_tiff_begin_page(gx_device_printer *pdev, gdev_tiff_state *tifs, FILE *fp,
  const TIFF_dir_entry *entries, int entry_count,
  const byte *values, int value_size)
{	TIFF_std_directory_entries std_entries;
	const TIFF_dir_entry *pse = (TIFF_dir_entry *)&std_entries;
	const TIFF_dir_entry *pce;
	TIFF_dir_entry entry;
#define std_entry_count\
  (sizeof(TIFF_std_directory_entries) / sizeof(TIFF_dir_entry))
	int nse, nce, ntags;
	TIFF_std_directory_values std_values;
#define std_value_size sizeof(TIFF_std_directory_values)

	if ( gdev_prn_file_is_new(pdev) )
	  {	/* This is a new file; write the TIFF header. */
		static const TIFF_header hdr =
		  {
#if arch_is_big_endian
		    TIFF_magic_big_endian,
#else
		    TIFF_magic_little_endian,
#endif
		    TIFF_version_value,
		    sizeof(TIFF_header)
		  };
		fwrite((const char *)&hdr, sizeof(hdr), 1, fp);
		tifs->prev_dir = 0;
	  }
	else
	  {	/* Patch pointer to this directory from previous. */
		TIFF_ulong offset = (TIFF_ulong)tifs->dir_off;
		fseek(fp, tifs->prev_dir, SEEK_SET);
		fwrite((char *)&offset, sizeof(offset), 1, fp);
		fseek(fp, tifs->dir_off, SEEK_SET);
	  }

	/* We're going to shuffle the two tag lists together. */
	/* Both lists are sorted; entries in the client list */
	/* replace entries with the same tag in the standard list. */
	for ( ntags = 0, pse = (const TIFF_dir_entry *)&std_entries,
	      nse = std_entry_count, pce = entries, nce = entry_count;
	      nse && nce; ++ntags
	    )
	  {	if ( pse->tag < pce->tag ) ++pse, --nse;
		else if ( pce->tag < pse->tag ) ++pce, --nce;
		else ++pse, --nse, ++pce, --nce;
	  }
	ntags += nse + nce;
	tifs->ntags = ntags;
	tifs->vsize = std_value_size + value_size;

	/* Write count of tags in directory. */
	{	TIFF_short dircount = ntags;
		fwrite((char *)&dircount, sizeof(dircount), 1, fp);
	}
	tifs->dir_off = ftell(fp);

	/* Fill in standard directory tags. */
	std_entries = std_entries_initial;
	std_values = std_values_initial;
	std_entries.ImageWidth.value = pdev->width;
	std_entries.ImageLength.value = pdev->height;
	std_entries.StripOffsets.value =
	  tifs->dir_off + sizeof(TIFF_std_directory_entries) +
	    entry_count * sizeof(TIFF_dir_entry) +
	    sizeof(TIFF_std_directory_values) + value_size;
	std_entries.RowsPerStrip.value = pdev->height;
	std_entries.PageNumber.value = (TIFF_ulong)pdev->PageCount;
	std_values.xresValue[0] = pdev->x_pixels_per_inch;
	std_values.yresValue[0] = pdev->y_pixels_per_inch;
	{	char revs[10];
		strncpy(std_values.softwareValue, gs_product, maxSoftware);
		std_values.softwareValue[maxSoftware - 1] = 0;
		sprintf(revs, " %1.2f", gs_revision / 100.0);
		strncat(std_values.softwareValue, revs,
			maxSoftware - strlen(std_values.softwareValue) - 1);
		std_entries.Software.count =
		  strlen(std_values.softwareValue) + 1;
	}
	{	struct tm tms;
		time_t t;
		time(&t);
		tms = *localtime(&t);
		sprintf(std_values.dateTimeValue,
			"%04d:%02d:%02d %02d:%02d:%02d",
			tms.tm_year + 1900, tms.tm_mon, tms.tm_mday,
			tms.tm_hour, tms.tm_min, tms.tm_sec);
	}

	/* Write the merged directory. */
	for ( pse = (const TIFF_dir_entry *)&std_entries,
	      nse = std_entry_count, pce = entries, nce = entry_count;
	      nse || nce;
	    )
	  {	bool std;
		if ( nce == 0 || (nse != 0 && pse->tag < pce->tag) )
		  std = true, entry = *pse++, --nse;
		else if ( nse == 0 || (nce != 0 && pce->tag < pse->tag) )
		  std = false, entry = *pce++, --nce;
		else
		  std = false, ++pse, --nse, entry = *pce++, --nce;
		if ( entry.tag == TIFFTAG_StripByteCounts )
		  tifs->offset_StripByteCounts =
		    (int)(ftell(fp) - tifs->dir_off);
		tiff_fixup_tag(&entry);	/* don't fix up indirects */
		if ( entry.type & TIFF_INDIRECT )
		  { /* Fix up the offset for an indirect value. */
		    entry.type -= TIFF_INDIRECT;
		    entry.value +=
		      tifs->dir_off + ntags * sizeof(TIFF_dir_entry) +
			(std ? 0 : std_value_size);
		  }
		fwrite((char *)&entry, sizeof(entry), 1, fp);
	  }

	/* Write the indirect values. */
	fwrite((const char *)&std_values, sizeof(std_values), 1, fp);
	fwrite((const char *)values, value_size, 1, fp);

	return 0;
}

/* End a TIFF page. */
int
gdev_tiff_end_page(gdev_tiff_state *tifs, FILE *fp)
{	long dir_off = tifs->dir_off;
	int tags_size = tifs->ntags * sizeof(TIFF_dir_entry);
	TIFF_ulong cc;

	tifs->prev_dir =
	  dir_off + tags_size + offset_of(TIFF_std_directory_values, diroff);
	tifs->dir_off = ftell(fp);
	/* Patch strip byte counts value. */
	cc = tifs->dir_off - (dir_off + tags_size + tifs->vsize);
	fseek(fp, dir_off + tifs->offset_StripByteCounts +
		offset_of(TIFF_dir_entry, value), SEEK_SET);
	fwrite(&cc, sizeof(cc), 1, fp);
	return 0;
}
