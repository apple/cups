/*
  Copyright 1993-1999 by Easy Software Products.
  Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.
  
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
#include <config.h>
#ifdef HAVE_LIBJPEG
/* sjpegc.c */
/* Interface routines for IJG code, common to encode/decode. */
#include "stdio_.h"
#include "string_.h"
#include "jpeglib.h"
#include "jerror.h"
#include "gx.h"
#include "gserrors.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"

/* gs_jpeg_message_table() is kept in a separate file for arcane reasons.
 * See sjpegerr.c.
 */
const char * const * gs_jpeg_message_table (P1(void));

/*
 * Error handling routines (these replace corresponding IJG routines from
 * jpeg/jerror.c).  These are used for both compression and decompression.
 * We assume
 * offset_of(jpeg_compress_data, cinfo)==offset_of(jpeg_decompress_data, dinfo)
 */

private void
gs_jpeg_error_exit (j_common_ptr cinfo)
{	jpeg_stream_data *jcomdp =
	  (jpeg_stream_data *)((char *)cinfo -
			       offset_of(jpeg_compress_data, cinfo));
	longjmp(jcomdp->exit_jmpbuf, 1);
}

private void
gs_jpeg_emit_message (j_common_ptr cinfo, int msg_level)
{	if ( msg_level < 0 )
	{	/* GS policy is to ignore IJG warnings when Picky=0,
		 * treat them as errors when Picky=1.
		 */
		jpeg_stream_data *jcomdp =
		  (jpeg_stream_data *)((char *)cinfo -
				       offset_of(jpeg_compress_data, cinfo));
		if ( jcomdp->Picky )
			gs_jpeg_error_exit(cinfo);
	}
	/* Trace messages are always ignored. */
}

/*
 * This is an exact copy of format_message from jpeg/jerror.c.
 * We do not use jerror.c in Ghostscript, so we have to duplicate this routine.
 */

private void
gs_jpeg_format_message (j_common_ptr cinfo, char * buffer)
{
  struct jpeg_error_mgr * err = cinfo->err;
  int msg_code = err->msg_code;
  const char * msgtext = NULL;
  const char * msgptr;
  char ch;
  boolean isstring;

  /* Look up message string in proper table */
  if (msg_code > 0 && msg_code <= err->last_jpeg_message) {
    msgtext = err->jpeg_message_table[msg_code];
  } else if (err->addon_message_table != NULL &&
	     msg_code >= err->first_addon_message &&
	     msg_code <= err->last_addon_message) {
    msgtext = err->addon_message_table[msg_code - err->first_addon_message];
  }

  /* Defend against bogus message number */
  if (msgtext == NULL) {
    err->msg_parm.i[0] = msg_code;
    msgtext = err->jpeg_message_table[0];
  }

  /* Check for string parameter, as indicated by %s in the message text */
  isstring = FALSE;
  msgptr = msgtext;
  while ((ch = *msgptr++) != '\0') {
    if (ch == '%') {
      if (*msgptr == 's') isstring = TRUE;
      break;
    }
  }

  /* Format the message into the passed buffer */
  if (isstring)
    sprintf(buffer, msgtext, err->msg_parm.s);
  else
    sprintf(buffer, msgtext,
	    err->msg_parm.i[0], err->msg_parm.i[1],
	    err->msg_parm.i[2], err->msg_parm.i[3],
	    err->msg_parm.i[4], err->msg_parm.i[5],
	    err->msg_parm.i[6], err->msg_parm.i[7]);
}

/* And this is an exact copy of another routine from jpeg/jerror.c. */

private void
gs_jpeg_reset_error_mgr (j_common_ptr cinfo)
{
  cinfo->err->num_warnings = 0;
  /* trace_level is not reset since it is an application-supplied parameter */
  cinfo->err->msg_code = 0;	/* may be useful as a flag for "no error" */
}

/*
 * This routine initializes the error manager fields in the JPEG object.
 * It is based on jpeg_std_error from jpeg/jerror.c.
 */

void
gs_jpeg_error_setup (stream_DCT_state *st)
{
  struct jpeg_error_mgr * err = &st->data.common->err;

  err->error_exit = gs_jpeg_error_exit;
  err->emit_message = gs_jpeg_emit_message;
  /* We need not set the output_message field since gs_jpeg_emit_message
   * doesn't call it, and the IJG library never calls output_message directly.
   * Setting the format_message field isn't strictly necessary either,
   * since gs_jpeg_log_error calls gs_jpeg_format_message directly.
   */
  err->format_message = gs_jpeg_format_message;
  err->reset_error_mgr = gs_jpeg_reset_error_mgr;

  err->trace_level = 0;		/* default = no tracing */
  err->num_warnings = 0;	/* no warnings emitted yet */
  err->msg_code = 0;		/* may be useful as a flag for "no error" */

  /* Initialize message table pointers */
  err->jpeg_message_table = gs_jpeg_message_table();
  err->last_jpeg_message = (int) JMSG_LASTMSGCODE - 1;

  err->addon_message_table = NULL;
  err->first_addon_message = 0;	/* for safety */
  err->last_addon_message = 0;

  st->data.compress->cinfo.err = err; /* works for decompress case too */
}

/* Stuff the IJG error message into errorinfo after an error exit. */

int
gs_jpeg_log_error (stream_DCT_state *st)
{	j_common_ptr cinfo = (j_common_ptr) &st->data.compress->cinfo;
	char buffer[JMSG_LENGTH_MAX];
	/* Format the error message */
	gs_jpeg_format_message(cinfo, buffer);
	(*st->report_error)((stream_state *)st, buffer);
	return gs_error_ioerror;	/* caller will do return_error() */
}


/*
 * Interface routines.  This layer of routines exists solely to limit
 * side-effects from using setjmp.
 */


JQUANT_TBL *
gs_jpeg_alloc_quant_table (stream_DCT_state *st)
{	if (setjmp(st->data.common->exit_jmpbuf))
	{	gs_jpeg_log_error(st);
		return NULL;
	}
	return jpeg_alloc_quant_table((j_common_ptr)
				      &st->data.compress->cinfo);
}

JHUFF_TBL *
gs_jpeg_alloc_huff_table (stream_DCT_state *st)
{	if (setjmp(st->data.common->exit_jmpbuf))
	{	gs_jpeg_log_error(st);
		return NULL;
	}
	return jpeg_alloc_huff_table((j_common_ptr)
				     &st->data.compress->cinfo);
}

int
gs_jpeg_destroy (stream_DCT_state *st)
{	if (setjmp(st->data.common->exit_jmpbuf))
		return_error(gs_jpeg_log_error(st));
	jpeg_destroy((j_common_ptr) &st->data.compress->cinfo);
	return 0;
}

#if 0
/*
 * These routines replace the low-level memory manager of the IJG library.
 * They pass malloc/free calls to the Ghostscript memory manager.
 * Note we do not need these to be declared in any GS header file.
 */

void *
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
  return gs_malloc(1, sizeofobject, "JPEG small internal data allocation");
}

void
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
  gs_free(object, 1, sizeofobject, "Freeing JPEG small internal data");
}

void FAR *
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
  return gs_malloc(1, sizeofobject, "JPEG large internal data allocation");
}

void
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
  gs_free(object, 1, sizeofobject, "Freeing JPEG large internal data");
}

long
jpeg_mem_available (j_common_ptr cinfo, long min_bytes_needed,
		    long max_bytes_needed, long already_allocated)
{
  return max_bytes_needed;
}

void
jpeg_open_backing_store (j_common_ptr cinfo, void * info,
			 long total_bytes_needed)
{
  ERREXIT(cinfo, JERR_NO_BACKING_STORE);
}

long
jpeg_mem_init (j_common_ptr cinfo)
{
  return 0;			/* just set max_memory_to_use to 0 */
}

void
jpeg_mem_term (j_common_ptr cinfo)
{
  /* no work */
}
#endif /* 0 */
#endif
