/* Copyright (C) 1993 Aladdin Enterprises.  All rights reserved.
  
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

/* zfname.c */
/* File name utilities */
#include "memory_.h"
#include "ghost.h"
#include "errors.h"
#include "oper.h"
#include "ialloc.h"
#include "stream.h"
#include "gxiodev.h"			/* must come after stream.h */
#include "fname.h"

/* Parse a file name into device and individual name. */
/* The device may be NULL, or the name may be NULL, but not both. */
/* According to the Adobe documentation, %device and %device% */
/* are equivalent; both return name==NULL. */
int
parse_file_name(const ref *op, parsed_file_name *pfn)
{	const byte *pname;
	uint len, dlen;
	const byte *pdelim;
	gx_io_device *iodev;

	check_read_type(*op, t_string);
	len = r_size(op);
	pname = op->value.const_bytes;
	if ( len == 0 )
		return_error(e_undefinedfilename);
	if ( pname[0] != '%' )	/* no device */
	  { pfn->iodev = NULL;
	    pfn->fname = (const char *)pname;
	    pfn->len = len;
	    return 0;
	  }
	pdelim = (const byte *)memchr(pname + 1, '%', len - 1);
	if ( pdelim == NULL )	/* %device */
	  dlen = len;
	else if ( pdelim[1] == 0 ) /* %device% */
	  { pdelim = NULL;
	    dlen = len;
	  }
	else
	  { dlen = pdelim - pname;
	    pdelim++, len--;
	  }
	iodev = gs_findiodevice(pname, dlen);
	if ( iodev == 0 )
	  return_error(e_undefinedfilename);
	pfn->iodev = iodev;
	pfn->fname = (const char *)pdelim;
	pfn->len = len - dlen;
	return 0;
}

/* Parse a real (non-device) file name and convert to a C string. */
int
parse_real_file_name(const ref *op, parsed_file_name *pfn, client_name_t cname)
{	int code = parse_file_name(op, pfn);
	if ( code < 0 )
		return code;
	if ( pfn->len == 0 )
		return_error(e_invalidfileaccess); /* device only */
	return terminate_file_name(pfn, cname);
}

/* Convert a file name to a C string by adding a null terminator. */
int
terminate_file_name(parsed_file_name *pfn, client_name_t cname)
{	uint len = pfn->len;
	ref fnref;
	const char *fname;
	if ( pfn->iodev == NULL )	 /* no device */
		pfn->iodev = iodev_default;
	fnref.value.const_bytes = (const byte *)pfn->fname;
	r_set_size(&fnref, len);
	fname = ref_to_string(&fnref, imemory, cname);
	if ( fname == 0 )
		return_error(e_VMerror);
	pfn->fname = fname;
	pfn->len = len + 1;	/* null terminator */
	return 0;
}

/* Free a file name that was copied to a C string. */
void
free_file_name(parsed_file_name *pfn, client_name_t cname)
{	if ( pfn->fname != 0 )
	  ifree_string((byte *)pfn->fname, pfn->len, cname);
}
