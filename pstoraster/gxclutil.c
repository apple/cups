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

/*$Id: gxclutil.c,v 1.2 2001/03/16 20:42:06 mike Exp $ */
/* Command list writing utilities. */

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"

/* ---------------- Statistics ---------------- */

#ifdef DEBUG
const char *const cmd_op_names[16] =
{cmd_op_name_strings};
private const char *const cmd_misc_op_names[16] =
{cmd_misc_op_name_strings};
private const char *const cmd_misc2_op_names[16] =
{cmd_misc2_op_name_strings};
private const char *const cmd_segment_op_names[16] =
{cmd_segment_op_name_strings};
private const char *const cmd_path_op_names[16] =
{cmd_path_op_name_strings};
const char *const *const cmd_sub_op_names[16] =
{cmd_misc_op_names, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0,
 0, cmd_misc2_op_names, cmd_segment_op_names, cmd_path_op_names
};
struct stats_cmd_s {
    ulong op_counts[256];
    ulong op_sizes[256];
    ulong tile_reset, tile_found, tile_added;
    ulong same_band, other_band;
} stats_cmd;
extern ulong stats_cmd_diffs[5];	/* in gxclpath.c */
int
cmd_count_op(int op, uint size)
{
    stats_cmd.op_counts[op]++;
    stats_cmd.op_sizes[op] += size;
    if (gs_debug_c('L')) {
	const char *const *sub = cmd_sub_op_names[op >> 4];

	if (sub)
	    dlprintf2(", %s(%u)\n", sub[op & 0xf], size);
	else
	    dlprintf3(", %s %d(%u)\n", cmd_op_names[op >> 4], op & 0xf,
		      size);
	fflush(dstderr);
    }
    return op;
}
void
cmd_uncount_op(int op, uint size)
{
    stats_cmd.op_counts[op]--;
    stats_cmd.op_sizes[op] -= size;
}
#endif

/* Print statistics. */
#ifdef DEBUG
void
cmd_print_stats(void)
{
    int ci, cj;

    dlprintf3("[l]counts: reset = %lu, found = %lu, added = %lu\n",
	      stats_cmd.tile_reset, stats_cmd.tile_found,
	      stats_cmd.tile_added);
    dlprintf5("     diff 2.5 = %lu, 3 = %lu, 4 = %lu, 2 = %lu, >4 = %lu\n",
	      stats_cmd_diffs[0], stats_cmd_diffs[1], stats_cmd_diffs[2],
	      stats_cmd_diffs[3], stats_cmd_diffs[4]);
    dlprintf2("     same_band = %lu, other_band = %lu\n",
	      stats_cmd.same_band, stats_cmd.other_band);
    for (ci = 0; ci < 0x100; ci += 0x10) {
	const char *const *sub = cmd_sub_op_names[ci >> 4];

	if (sub != 0) {
	    dlprintf1("[l]  %s =", cmd_op_names[ci >> 4]);
	    for (cj = ci; cj < ci + 0x10; cj += 2)
		dprintf6("\n\t%s = %lu(%lu), %s = %lu(%lu)",
			 sub[cj - ci],
			 stats_cmd.op_counts[cj], stats_cmd.op_sizes[cj],
			 sub[cj - ci + 1],
		   stats_cmd.op_counts[cj + 1], stats_cmd.op_sizes[cj + 1]);
	} else {
	    ulong tcounts = 0, tsizes = 0;

	    for (cj = ci; cj < ci + 0x10; cj++)
		tcounts += stats_cmd.op_counts[cj],
		    tsizes += stats_cmd.op_sizes[cj];
	    dlprintf3("[l]  %s (%lu,%lu) =\n\t",
		      cmd_op_names[ci >> 4], tcounts, tsizes);
	    for (cj = ci; cj < ci + 0x10; cj++)
		if (stats_cmd.op_counts[cj] == 0)
		    dputs(" -");
		else
		    dprintf2(" %lu(%lu)", stats_cmd.op_counts[cj],
			     stats_cmd.op_sizes[cj]);
	}
	dputs("\n");
    }
}
#endif /* DEBUG */

/* ---------------- Writing utilities ---------------- */

/* Write the commands for one band or band range. */
private int	/* ret 0 all ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_band(gx_device_clist_writer * cldev, int band_min, int band_max,
	       cmd_list * pcl, byte cmd_end)
{
    const cmd_prefix *cp = pcl->head;
    int code_b = 0;
    int code_c = 0;

    if (cp != 0 || cmd_end != cmd_opv_end_run) {
	clist_file_ptr cfile = cldev->page_cfile;
	clist_file_ptr bfile = cldev->page_bfile;
	cmd_block cb;
	byte end = cmd_count_op(cmd_end, 1);

	if (cfile == 0 || bfile == 0)
 	    return_error(gs_error_ioerror);
	cb.band_min = band_min;
	cb.band_max = band_max;
	cb.pos = clist_ftell(cfile);
	if_debug3('l', "[l]writing for bands (%d,%d) at %ld\n",
		  band_min, band_max, cb.pos);
	clist_fwrite_chars(&cb, sizeof(cb), bfile);
	if (cp != 0) {
	    pcl->tail->next = 0;	/* terminate the list */
	    for (; cp != 0; cp = cp->next) {
#ifdef DEBUG
		if ((const byte *)cp < cldev->cbuf ||
		    (const byte *)cp >= cldev->cend ||
		    cp->size > cldev->cend - (const byte *)cp
		    ) {
		    lprintf1("cmd_write_band error at 0x%lx\n", (ulong) cp);
		    return_error(gs_error_Fatal);
		}
#endif
		clist_fwrite_chars(cp + 1, cp->size, cfile);
	    }
	    pcl->head = pcl->tail = 0;
	}
	clist_fwrite_chars(&end, 1, cfile);
	process_interrupts();
	code_b = clist_ferror_code(bfile);
	code_c = clist_ferror_code(cfile);
	if (code_b < 0)
	    return_error(code_b);
	if (code_c < 0)
	    return_error(code_c); 
    }
    return code_b | code_c;
}

/* Write out the buffered commands, and reset the buffer. */
int	/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_buffer(gx_device_clist_writer * cldev, byte cmd_end)
{
    int nbands = cldev->nbands;
    gx_clist_state *pcls;
    int band;
    int code = cmd_write_band(cldev, cldev->band_range_min,
			      cldev->band_range_max,
			      &cldev->band_range_list, cmd_opv_end_run);
    int warning = code;

    for (band = 0, pcls = cldev->states;
	 code >= 0 && band < nbands; band++, pcls++
	 ) {
	code = cmd_write_band(cldev, band, band, &pcls->list, cmd_end);
	warning |= code;
    }
    /* If an error occurred, finish cleaning up the pointers. */
    for (; band < nbands; band++, pcls++)
	pcls->list.head = pcls->list.tail = 0;
    cldev->cnext = cldev->cbuf;
    cldev->ccl = 0;
#ifdef DEBUG
    if (gs_debug_c('l'))
	cmd_print_stats();
#endif
    return_check_interrupt(code != 0 ? code : warning);
}

/*
 * Add a command to the appropriate band list, and allocate space for its
 * data.  Return the pointer to the data area.  If an error or (low-memory
 * warning) occurs, set cldev->error_code and return 0.
 */
#define cmd_headroom (sizeof(cmd_prefix) + arch_align_ptr_mod)
byte *
cmd_put_list_op(gx_device_clist_writer * cldev, cmd_list * pcl, uint size)
{
    byte *dp = cldev->cnext;

    if (size + cmd_headroom > cldev->cend - dp) {
	if ((cldev->error_code =
	       cmd_write_buffer(cldev, cmd_opv_end_run)) != 0) {
	    if (cldev->error_code < 0)
		cldev->error_is_retryable = 0;	/* hard error */
	    else {
		/* upgrade lo-mem warning into an error */
		if (!cldev->ignore_lo_mem_warnings)
		    cldev->error_code = gs_note_error(gs_error_VMerror);
		cldev->error_is_retryable = 1;
	    }
	    return 0;
	}
	else
	    return cmd_put_list_op(cldev, pcl, size);
    }
    if (cldev->ccl == pcl) {	/* We're adding another command for the same band. */
	/* Tack it onto the end of the previous one. */
	cmd_count_add1(stats_cmd.same_band);
#ifdef DEBUG
	if (pcl->tail->size > dp - (byte *) (pcl->tail + 1)) {
	    lprintf1("cmd_put_list_op error at 0x%lx\n", (ulong) pcl->tail);
	}
#endif
	pcl->tail->size += size;
    } else {
	/* Skip to an appropriate alignment boundary. */
	/* (We assume the command buffer itself is aligned.) */
	cmd_prefix *cp = (cmd_prefix *)
	    (dp + ((cldev->cbuf - dp) & (arch_align_ptr_mod - 1)));

	cmd_count_add1(stats_cmd.other_band);
	dp = (byte *) (cp + 1);
	if (pcl->tail != 0) {
#ifdef DEBUG
	    if (pcl->tail < pcl->head ||
		pcl->tail->size > dp - (byte *) (pcl->tail + 1)
		) {
		lprintf1("cmd_put_list_op error at 0x%lx\n",
			 (ulong) pcl->tail);
	    }
#endif
	    pcl->tail->next = cp;
	} else
	    pcl->head = cp;
	pcl->tail = cp;
	cldev->ccl = pcl;
	cp->size = size;
    }
    cldev->cnext = dp + size;
    return dp;
}
#ifdef DEBUG
byte *
cmd_put_op(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size)
{
    if_debug3('L', "[L]band %d: size=%u, left=%u",
	      (int)(pcls - cldev->states),
	      size, (uint) (cldev->cend - cldev->cnext));
    return cmd_put_list_op(cldev, &pcls->list, size);
}
#endif

/* Add a command for a range of bands. */
byte *
cmd_put_range_op(gx_device_clist_writer * cldev, int band_min, int band_max,
		 uint size)
{
    if_debug4('L', "[L]band range(%d,%d): size=%u, left=%u",
	      band_min, band_max, size,
	      (uint)(cldev->cend - cldev->cnext));
    if (cldev->ccl != 0 && 
	(cldev->ccl != &cldev->band_range_list ||
	 band_min != cldev->band_range_min ||
	 band_max != cldev->band_range_max)
	) {
	if ((cldev->error_code = cmd_write_buffer(cldev, cmd_opv_end_run)) != 0) {
	    if (cldev->error_code < 0)
		cldev->error_is_retryable = 0;	/* hard error */
	    else {
		/* upgrade lo-mem warning into an error */
		cldev->error_code = gs_error_VMerror;
		cldev->error_is_retryable = 1;
	    }
	    return 0;
	}
	cldev->band_range_min = band_min;
	cldev->band_range_max = band_max;
    }
    return cmd_put_list_op(cldev, &cldev->band_range_list, size);
}

/* Write a variable-size positive integer. */
int
cmd_size_w(register uint w)
{
    register int size = 1;

    while (w > 0x7f)
	w >>= 7, size++;
    return size;
}
byte *
cmd_put_w(register uint w, register byte * dp)
{
    while (w > 0x7f)
	*dp++ = w | 0x80, w >>= 7;
    *dp = w;
    return dp + 1;
}

/* Define the encodings of the different settable colors. */
const clist_select_color_t
    clist_select_color0 = {cmd_op_set_color0, cmd_opv_delta2_color0, 0},
    clist_select_color1 = {cmd_op_set_color1, cmd_opv_delta2_color1, 0},
    clist_select_tile_color0 = {cmd_op_set_color0, cmd_opv_delta2_color0, 1},
    clist_select_tile_color1 = {cmd_op_set_color1, cmd_opv_delta2_color1, 1};
int
cmd_put_color(gx_device_clist_writer * cldev, gx_clist_state * pcls,
	      const clist_select_color_t * select,
	      gx_color_index color, gx_color_index * pcolor)
{
    byte *dp;
    long diff = (long)color - (long)(*pcolor);
    byte op, op_delta2;
    int code;

    if (diff == 0)
	return 0;
    if (select->tile_color) {
	code = set_cmd_put_op(dp, cldev, pcls, cmd_opv_set_tile_color, 1);
	if (code < 0)
	    return code;
    }
    op = select->set_op;
    op_delta2 = select->delta2_op;
    if (color == gx_no_color_index) {
	/*
	 * We must handle this specially, because it may take more
	 * bytes than the color depth.
	 */
	code = set_cmd_put_op(dp, cldev, pcls, op + 15, 1);
	if (code < 0)
	    return code;
    } else {
	long delta;
	byte operand;

	switch ((cldev->color_info.depth + 15) >> 3) {
	    case 5:
		if (!((delta = diff + cmd_delta1_32_bias) &
		      ~cmd_delta1_32_mask) &&
		    (operand =
		     (byte) ((delta >> 23) + ((delta >> 18) & 1))) != 0 &&
		    operand != 15
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  (byte) (op + operand), 2);
		    if (code < 0)
			return code;
		    dp[1] = (byte) (((delta >> 10) & 0300) +
				    (delta >> 5) + delta);
		    break;
		}
		if (!((delta = diff + cmd_delta2_32_bias) &
		      ~cmd_delta2_32_mask)
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
		    if (code < 0)
			return code;
		    dp[1] = (byte) ((delta >> 20) + (delta >> 16));
		    dp[2] = (byte) ((delta >> 4) + delta);
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 5);
		if (code < 0)
		    return code;
		*++dp = (byte) (color >> 24);
		goto b3;
	    case 4:
		if (!((delta = diff + cmd_delta1_24_bias) &
		      ~cmd_delta1_24_mask) &&
		    (operand = (byte) (delta >> 16)) != 0 &&
		    operand != 15
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  (byte) (op + operand), 2);
		    if (code < 0)
			return code;
		    dp[1] = (byte) ((delta >> 4) + delta);
		    break;
		}
		if (!((delta = diff + cmd_delta2_24_bias) &
		      ~cmd_delta2_24_mask)
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
		    if (code < 0)
			return code;
		    dp[1] = ((byte) (delta >> 13) & 0xf8) +
			((byte) (delta >> 11) & 7);
		    dp[2] = (byte) (((delta >> 3) & 0xe0) + delta);
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 4);
		if (code < 0)
		    return code;
b3:		*++dp = (byte) (color >> 16);
		goto b2;
	    case 3:
		code = set_cmd_put_op(dp, cldev, pcls, op, 3);
		if (code < 0)
		    return code;
b2:		*++dp = (byte) (color >> 8);
		goto b1;
	    case 2:
		if (diff >= -7 && diff < 7) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  op + (int)diff + 8, 1);
		    if (code < 0)
			return code;
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 2);
		if (code < 0)
		    return code;
b1:		dp[1] = (byte) color;
	}
    }
    *pcolor = color;
    return 0;
}

/* Put out a command to set the tile colors. */
int
cmd_set_tile_colors(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		    gx_color_index color0, gx_color_index color1)
{
    int code = 0;

    if (color0 != pcls->tile_colors[0]) {
	code = cmd_put_color(cldev, pcls,
			     &clist_select_tile_color0,
			     color0, &pcls->tile_colors[0]);
	if (code != 0)
	    return code;
    }
    if (color1 != pcls->tile_colors[1])
	code = cmd_put_color(cldev, pcls,
			     &clist_select_tile_color1,
			     color1, &pcls->tile_colors[1]);
    return code;
}

/* Put out a command to set the tile phase. */
int
cmd_set_tile_phase(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		   int px, int py)
{
    int pcsize;
    byte *dp;
    int code;

    pcsize = 1 + cmd_size2w(px, py);
    code =
	set_cmd_put_op(dp, cldev, pcls, (byte)cmd_opv_set_tile_phase, pcsize);
    if (code < 0)
	return code;
    ++dp;
    pcls->tile_phase.x = px;
    pcls->tile_phase.y = py;
    cmd_putxy(pcls->tile_phase, dp);
    return 0;
}

/* Write a command to enable or disable the logical operation. */
int
cmd_put_enable_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		   int enable)
{
    byte *dp;
    int code = set_cmd_put_op(dp, cldev, pcls,
			      (byte)(enable ? cmd_opv_enable_lop :
				     cmd_opv_disable_lop),
			      1);

    if (code < 0)
	return code;
    pcls->lop_enabled = enable;
    return 0;
}

/* Write a command to enable or disable clipping. */
/* This routine is only called if the path extensions are included. */
int
cmd_put_enable_clip(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		    int enable)
{
    byte *dp;
    int code = set_cmd_put_op(dp, cldev, pcls,
			      (byte)(enable ? cmd_opv_enable_clip :
				     cmd_opv_disable_clip),
			      1);

    if (code < 0)
	return code;
    pcls->clip_enabled = enable;
    return 0;
}

/* Write a command to set the logical operation. */
int
cmd_set_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
	    gs_logical_operation_t lop)
{
    byte *dp;
    uint lop_msb = lop >> 6;
    int code = set_cmd_put_op(dp, cldev, pcls,
			      cmd_opv_set_misc, 2 + cmd_size_w(lop_msb));

    if (code < 0)
	return code;
    dp[1] = cmd_set_misc_lop + (lop & 0x3f);
    cmd_put_w(lop_msb, dp + 2);
    pcls->lop = lop;
    return 0;
}

/* Disable (if default) or enable the logical operation, setting it if */
/* needed. */
int
cmd_update_lop(gx_device_clist_writer *cldev, gx_clist_state *pcls,
	       gs_logical_operation_t lop)
{
    int code;

    if (lop == lop_default)
	return cmd_disable_lop(cldev, pcls);
    code = cmd_set_lop(cldev, pcls, lop);
    if (code < 0)
	return code;
    return cmd_enable_lop(cldev, pcls);
}

/* Write a parameter list */
int	/* ret 0 all ok, -ve error */
cmd_put_params(gx_device_clist_writer *cldev,
	       gs_param_list *param_list) /* NB open for READ */
{
    byte *dp;
    int code;
    byte local_buf[512];	/* arbitrary */
    int param_length;

    /* Get serialized list's length + try to get it into local var if it fits. */
    param_length = code =
	gs_param_list_serialize(param_list, local_buf, sizeof(local_buf));
    if (param_length > 0) {
	/* Get cmd buffer space for serialized */
	code = set_cmd_put_all_op(dp, cldev, cmd_opv_put_params,
				  1 + sizeof(unsigned) + param_length);
	if (code < 0)
	    return code;

	/* write param list to cmd list: needs to all fit in cmd buffer */
	if_debug1('l', "[l]put_params, length=%d\n", param_length);
	++dp;
	memcpy(dp, &param_length, sizeof(unsigned));
	dp += sizeof(unsigned);
	if (param_length > sizeof(local_buf)) {
	    int old_param_length = param_length;

	    param_length = code =
		gs_param_list_serialize(param_list, dp, old_param_length);
	    if (param_length >= 0)
		code = (old_param_length != param_length ?
			gs_note_error(gs_error_unknownerror) : 0);
	    if (code < 0) {
		/* error serializing: back out by writing a 0-length parm list */
		memset(dp - sizeof(unsigned), 0, sizeof(unsigned));
		cmd_shorten_list_op(cldev, &cldev->band_range_list,
				    old_param_length);
	    }
	} else
	    memcpy(dp, local_buf, param_length);	    /* did this when computing length */
    }
    return code;
}
