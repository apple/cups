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
/* pdfmark processing for PDF-writing driver */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsutil.h"		/* for bytes_compare */
#include "gdevpdfx.h"

/* GC descriptors */
private_st_pdf_article();
private_st_pdf_named_dest();

/*
 * The pdfmark pseudo-parameter indicates the occurrence of a pdfmark
 * operator in the input file.  Its "value" is the arguments of the operator,
 * passed through essentially unchanged:
 *	(key, value)*, type
 */

/* Define the pdfmark types we know about. */
#define pdfmark_proc(proc)\
  int proc(P3(gx_device_pdf *, const gs_param_string *, uint))
typedef struct pdfmark_name_s {
  const char *mname;
  pdfmark_proc((*proc));
} pdfmark_name;
private pdfmark_proc(pdfmark_ANN);
private pdfmark_proc(pdfmark_LNK);
private pdfmark_proc(pdfmark_OUT);
private pdfmark_proc(pdfmark_ARTICLE);
private pdfmark_proc(pdfmark_DEST);
private pdfmark_proc(pdfmark_PS);
private pdfmark_proc(pdfmark_PAGES);
private pdfmark_proc(pdfmark_PAGE);
private pdfmark_proc(pdfmark_DOCINFO);
private pdfmark_proc(pdfmark_DOCVIEW);
private const pdfmark_name mark_names[] = {
  { "ANN", pdfmark_ANN },
  { "LNK", pdfmark_LNK },
  { "OUT", pdfmark_OUT },
  { "ARTICLE", pdfmark_ARTICLE },
  { "DEST", pdfmark_DEST },
  { "PS", pdfmark_PS },
  { "PAGES", pdfmark_PAGES },
  { "PAGE", pdfmark_PAGE },
  { "DOCINFO", pdfmark_DOCINFO },
  { "DOCVIEW", pdfmark_DOCVIEW },
  { 0, 0 }
};

/* Process a pdfmark. */
int
pdfmark_process(gx_device_pdf *pdev, const gs_param_string_array *pma)
{	const gs_param_string *pts = &pma->data[pma->size - 1];
	int i;

	if ( !(pma->size & 1) )
	  return_error(gs_error_rangecheck);
	for ( i = 0; mark_names[i].mname != 0; ++i )
	  if ( pdf_key_eq(pts, mark_names[i].mname) )
	    return (*mark_names[i].proc)(pdev, pma->data, pma->size - 1);
	return 0;
}

/* Find a key in a dictionary. */
private bool
pdfmark_find_key(const char *key, const gs_param_string *pairs, uint count,
  gs_param_string *pstr)
{	uint i;
	for ( i = 0; i < count; i += 2 )
	  if ( pdf_key_eq(&pairs[i], key) )
	    { *pstr = pairs[i + 1];
	      return true;
	    }
	pstr->data = 0;
	pstr->size = 0;
	return false;
}

/* Get the ID for a page referenced by number or as /Next or /Prev. */
/* The result may be 0 if the page number is 0 or invalid. */
private long
pdfmark_page_id(gx_device_pdf *pdev, int *ppage, const gs_param_string *pnstr)
{	int page = pdev->next_page + 1;

	if ( pnstr->data == 0 )
	  ;
	else if ( pdf_key_eq(pnstr, "/Next") )
	  ++page;
	else if ( pdf_key_eq(pnstr, "/Prev") )
	  --page;
	else if ( sscanf((const char *)pnstr->data, "%d", &page) != 1 )	/****** WRONG, assumes null terminator ******/
	  page = 0;
	*ppage = page;
	return pdf_page_id(pdev, page);
}

/* Construct a destination string specified by /Page and/or /View. */
/* Return 0 if none (but still fill in a default), 1 if present, */
/* <0 if error. */
private int
pdfmark_make_dest(char dstr[max_dest_string], gx_device_pdf *pdev,
  const gs_param_string *pairs, uint count)
{	gs_param_string page_string, view_string;
	int page;
	bool present =
	  pdfmark_find_key("Page", pairs, count, &page_string) |
	    pdfmark_find_key("View", pairs, count, &view_string);
	long page_id = pdfmark_page_id(pdev, &page, &page_string);
	int len;

	if ( view_string.size == 0 )
	  param_string_from_string(view_string, "[/XYZ 0 0 1]");
	if ( page_id == 0 )
	  strcpy(dstr, "[null ");
	else
	  sprintf(dstr, "[%ld 0 R ", page_id);
	len = strlen(dstr);
	if ( len + view_string.size > max_dest_string )
	  return_error(gs_error_limitcheck);
	if ( view_string.data[0] != '[' ||
	     view_string.data[view_string.size - 1] != ']'
	   )
	  return_error(gs_error_rangecheck);
	memcpy(dstr + len, view_string.data + 1, view_string.size - 1);
	dstr[len + view_string.size - 1] = 0;
	return present;
}

/* Write pairs for a dictionary. */
private void
pdfmark_write_pair(FILE *file, const gs_param_string *pair)
{	fputc('/', file);
	fwrite(pair->data, 1, pair->size, file);
	fputc(' ', file);
	fwrite(pair[1].data, 1, pair[1].size, file);
	fputc('\n', file);
}

/* Copy an annotation dictionary. */
private int
pdfmark_annot(gx_device_pdf *pdev, const gs_param_string *pairs, uint count,
  const char *subtype)
{	FILE *file = pdev->file;
	pdf_resource *pres;
	int code = pdf_begin_aside(pdev, &pdev->annots, NULL, &pres);
	bool subtype_present = false;
	bool add_dest = false;
	bool dest_present = false;
	uint i;

	if ( code < 0 )
	  return code;
	pres->rid = pdev->next_page;
	fputs("<< /Type /Annot\n", file);
	for ( i = 0; i < count; i += 2 )
	  { const gs_param_string *pair = &pairs[i];
	    long src_pg;
	    if ( pdf_key_eq(pair, "SrcPg") &&
		 sscanf((const char *)pair[1].data, "%ld", &src_pg) == 1
	       )
	      pres->rid = src_pg - 1;
	    else if ( pdf_key_eq(pair, "Page") || pdf_key_eq(pair, "View") )
	      add_dest = true;
	    else
	      { pdfmark_write_pair(file, pair);
	        if ( pdf_key_eq(pair, "Dest") )
		  dest_present = true;
		else if ( pdf_key_eq(pair, "Subtype") )
		  subtype_present = true;
	      }
	  }
	if ( add_dest && !dest_present )
	  { char dest[max_dest_string];
	    int code = pdfmark_make_dest(dest, pdev, pairs, count);
	    if ( code >= 0 )
	      fprintf(file, "/Dest %s\n", dest);
	  }
	if ( !subtype_present )
	  fprintf(file, "/Subtype /%s ", subtype);
	fputs(">>\n", file);
	pdf_end_aside(pdev);
	return 0;
}

/* ANN pdfmark */
private int
pdfmark_ANN(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	return pdfmark_annot(pdev, pairs, count, "Text");
}

/* LNK pdfmark (obsolescent) */
private int
pdfmark_LNK(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	return pdfmark_annot(pdev, pairs, count, "Link");
}

/* Save pairs in a string. */
private bool
pdf_key_member(const gs_param_string *pcs, const char **keys)
{	const char **pkey;
	for ( pkey = keys; *pkey != 0; ++pkey )
	  if ( pdf_key_eq(pcs, *pkey) )
	    return true;
	return false;
}
private int
pdfmark_save_edited_pairs(const gx_device_pdf *pdev,
  const gs_param_string *pairs, uint count, const char **skip_keys,
  const gs_param_string *add_pairs, uint add_count, gs_string *pstr)
{	uint i, size;
	byte *data;
	byte *next;

	for ( i = 0, size = 0; i < count; i += 2 )
	  if ( !pdf_key_member(&pairs[i], skip_keys) )
	    size += pairs[i].size + pairs[i+1].size + 3;
	for ( i = 0; i < add_count; i += 2 )
	  size += add_pairs[i].size + add_pairs[i+1].size + 3;
	if ( pstr->data == 0 )
	  data = gs_alloc_string(pdev->pdf_memory, size, "pdfmark_save_pairs");
	else
	  data = gs_resize_string(pdev->pdf_memory, pstr->data, pstr->size,
				  size, "pdfmark_save_pairs");
	if ( data == 0 )
	  return_error(gs_error_VMerror);
	next = data;
	for ( i = 0; i < count; i += 2 )
	  if ( !pdf_key_member(&pairs[i], skip_keys) )
	    { uint len;
	      *next++ = '/';
	      memcpy(next, pairs[i].data, len = pairs[i].size);
	      next += len;
	      *next++ = ' ';
	      memcpy(next, pairs[i+1].data, len = pairs[i+1].size);
	      next += len;
	      *next++ = '\n';
	    }
	for ( i = 0; i < add_count; i += 2 )
	  { uint len;
	    *next++ = '/';
	    memcpy(next, add_pairs[i].data, len = add_pairs[i].size);
	    next += len;
	    *next++ = ' ';
	    memcpy(next, add_pairs[i+1].data, len = add_pairs[i+1].size);
	    next += len;
	    *next++ = '\n';
	  }
	pstr->data = data;
	pstr->size = size;
	return 0;
}
static const char *no_skip_pairs[] = { 0 };
#define pdfmark_save_pairs(pdev, pairs, count, pstr)\
  pdfmark_save_edited_pairs(pdev, pairs, count, no_skip_pairs, NULL, 0, pstr)

/* Write out one node of the outline tree. */
private int
pdfmark_write_outline(gx_device_pdf *pdev, pdf_outline_node *pnode,
  long next_id)
{	FILE *file = pdev->file;

	pdf_close_contents(pdev, false);
	pdf_open_obj(pdev, pnode->id);
	fputs("<< ", file);
	pdf_write_saved_string(pdev, &pnode->action_string);
	fprintf(file, "/Parent %ld 0 R\n", pnode->parent_id);
	if ( pnode->prev_id )
	  fprintf(file, "/Prev %ld 0 R\n", pnode->prev_id);
	if ( next_id )
	  fprintf(file, "/Next %ld 0 R\n", next_id);
	if ( pnode->first_id )
	  fprintf(file, "/First %ld 0 R /Last %ld 0 R\n",
		  pnode->first_id, pnode->last_id);
	fputs(">>\n", file);
	pdf_end_obj(pdev);
	return 0;
}

/* Close the current level of the outline tree. */
int
pdfmark_close_outline(gx_device_pdf *pdev)
{	int depth = pdev->outline_depth;
	pdf_outline_level *plevel = &pdev->outline_levels[depth];
	int code;

	code = pdfmark_write_outline(pdev, &plevel->last, 0);
	if ( code < 0 )
	  return code;
	if ( depth > 0 )
	  { plevel[-1].last.last_id = plevel->last.id;
	    if ( plevel[-1].last.count < 0 )
	      pdev->closed_outline_depth--;
	    pdev->outline_depth--;
	  }
	return 0;
}

/* OUT pdfmark */
private int
pdfmark_OUT(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	int depth = pdev->outline_depth;
	pdf_outline_level *plevel = &pdev->outline_levels[depth];
	int sub_count = 0;
	uint i;
	pdf_outline_node node;
	int code;

	for ( i = 0; i < count; i += 2 )
	  { const gs_param_string *pair = &pairs[i];
	    if ( pdf_key_eq(pair, "Count") )
	      sscanf((const char *)pair[1].data, "%d", &sub_count);
	  }
	if ( sub_count != 0 && pdev->outline_depth == max_outline_depth - 1 )
	  return_error(gs_error_limitcheck);
	node.action_string.data = 0;
	code = pdfmark_save_pairs(pdev, pairs, count, &node.action_string);
	if ( code < 0 )
	  return code;
	if ( pdev->outlines_id == 0 )
	  pdev->outlines_id = pdf_obj_ref(pdev);
	node.id = pdf_obj_ref(pdev);
	node.parent_id =
	  (depth == 0 ? pdev->outlines_id : plevel[-1].last.id);
	node.prev_id = plevel->last.id;
	node.first_id = node.last_id = 0;
	node.count = sub_count;
	/* Add this node to the outline at the current level. */
	if ( plevel->first.id == 0 )
	  { /* First node at this level. */
	    if ( depth > 0 )
	      plevel[-1].last.first_id = node.id;
	    node.prev_id = 0;
	    plevel->first = node;
	  }
	else
	  { /* Write out the previous node. */
	    pdfmark_write_outline(pdev, &plevel->last, node.id);
	  }
	plevel->last = node;
	plevel->left--;
	if ( !pdev->closed_outline_depth )
	  pdev->outlines_open++;
	/* If this node has sub-nodes, descend one level. */
	if ( sub_count != 0 )
	  { pdev->outline_depth = ++depth;
	    ++plevel;
	    plevel->left = (sub_count > 0 ? sub_count : -sub_count);
	    plevel->first.id = 0;
	    if ( sub_count < 0 )
	      pdev->closed_outline_depth++;
	  }
	else
	  { for ( ; depth > 0 && plevel->left == 0; --depth, --plevel )
	      { pdfmark_close_outline(pdev);
		plevel[-1].left--;
	      }
	  }
	return 0;
}

/* Write an article bead. */
int
pdfmark_write_article(gx_device_pdf *pdev, const pdf_bead *pbead)
{	FILE *file = pdev->file;

	pdf_open_obj(pdev, pbead->id);
	fprintf(file,
		"<<\n/T %ld 0 R\n/V %ld 0 R\n/N %ld 0 R\n",
		pbead->article_id, pbead->prev_id, pbead->next_id);
	fprintf(file, "/Dest %s\n", pbead->dest);
	fwrite(pbead->rect.data, 1, pbead->rect.size, file);
	fputs("\n>>\n", file);
	return pdf_end_obj(pdev);
}

/* ARTICLE pdfmark */
private int
pdfmark_ARTICLE(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	gs_param_string title;
	gs_param_string rect;
	long bead_id;
	pdf_article *part;

	if ( !pdfmark_find_key("Title", pairs, count, &title) ||
	     !pdfmark_find_key("Rect", pairs, count, &rect)
	   )
	  return_error(gs_error_rangecheck);
	/****** Should save other keys for Info dictionary ******/
	pdf_close_contents(pdev, false);

	/* Find the article with this title, or create one. */
	bead_id = pdf_obj_ref(pdev);
	for ( part = pdev->articles; part != 0; part = part->next )
	  if ( !bytes_compare(part->title.data, part->title.size,
			      title.data, title.size) )
	    break;
	if ( part == 0 )
	  { /* Create the article. */
	    FILE *file = pdev->file;
	    byte *str;

	    part = gs_alloc_struct(pdev->pdf_memory, pdf_article,
				   &st_pdf_article, "pdfmark_ARTICLE");
	    str = gs_alloc_string(pdev->pdf_memory, title.size,
				  "article title");
	    if ( part == 0 || str == 0 )
	      return_error(gs_error_VMerror);
	    part->next = pdev->articles;
	    pdev->articles = part;
	    memcpy(str, title.data, title.size);
	    part->title.data = str;
	    part->title.size = title.size;
	    part->id = pdf_begin_obj(pdev);
	    part->first.id = part->last.id = 0;
	    fprintf(file, "<< /F %ld 0 R >>\n", bead_id);
	    pdf_end_obj(pdev);
	  }

	/* Add the bead to the article. */
	/* This is similar to what we do for outline nodes. */
	if ( part->last.id == 0 )
	  { part->first.next_id = bead_id;
	    part->last.id = part->first.id;
	  }
	else
	  { part->last.next_id = bead_id;
	    pdfmark_write_article(pdev, &part->last);
	  }
	part->last.prev_id = part->last.id;
	part->last.id = bead_id;
	part->last.article_id = part->id;
	part->last.next_id = 0;
	part->last.rect.data = (byte *)rect.data;
	part->last.rect.size = rect.size;
	pdfmark_make_dest(part->last.dest, pdev, pairs, count);
	if ( part->first.id == 0 )
	  { /* This is the first bead of the article. */
	    part->first = part->last;
	    part->last.id = 0;
	  }

	return 0;
}

/* DEST pdfmark */
private int
pdfmark_DEST(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	char dest[max_dest_string];
	gs_param_string key;
	pdf_named_dest *pnd;
	byte *str;

	if ( !pdfmark_find_key("Dest", pairs, count, &key) ||
	     !pdfmark_make_dest(dest, pdev, pairs, count)
	   )
	  return_error(gs_error_rangecheck);
	pnd = gs_alloc_struct(pdev->pdf_memory, pdf_named_dest,
			      &st_pdf_named_dest, "pdfmark_DEST");
	str = gs_alloc_string(pdev->pdf_memory, key.size,
			      "named_dest key");
	if ( pnd == 0 || str == 0 )
	  return_error(gs_error_VMerror);
	pnd->next = pdev->named_dests;
	memcpy(str, key.data, key.size);
	pnd->key.data = str;
	pnd->key.size = key.size;
	strcpy(pnd->dest, dest);
	pdev->named_dests = pnd;
	return 0;
}

/* Write the contents of pass-through code. */
/* We are inside the stream dictionary. */
private int
pdfmark_write_ps(gx_device_pdf *pdev, const gs_param_string *psource)
{	FILE *file = pdev->file;
	long length_id = pdf_obj_ref(pdev);
	long start_pos, length;

	fprintf(file, " /Length %ld 0 R >> stream\n", length_id);
	start_pos = ftell(file);
	if ( psource->size < 2 || psource->data[0] != '(' ||
	     psource->data[psource->size - 1] != ')'
	   )
	  lprintf1("bad PS passthrough: %s\n", psource->data);
	else
	  { /****** SHOULD REMOVE ESCAPES ******/
	    fwrite(psource->data + 1, 1, psource->size - 2, file);
	  }
	fputs("\n", file);
	length = ftell(file) - start_pos;
	fputs("endstream\n", file);
	pdf_end_obj(pdev);
	pdf_open_obj(pdev, length_id);
	fprintf(file, "%ld\n", length);
	pdf_end_obj(pdev);
	return 0;
}

/* PS pdfmark */
private int
pdfmark_PS(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	FILE *file = pdev->file;
	gs_param_string source;
	gs_param_string level1;

	if ( !pdfmark_find_key("DataSource", pairs, count, &source) )
	  return_error(gs_error_rangecheck);
	pdfmark_find_key("Level1", pairs, count, &level1);
	if ( level1.data == 0 && source.size <= 100 )
	  { /* Insert the PostScript code in-line */
	    int code = pdf_begin_contents(pdev);
	    if ( code < 0 )
	      return code;
	    fwrite(source.data, 1, source.size, file);
	    fputs(" PS\n", file);
	  }
	else
	  { /* Put the PostScript code in a resource. */
	    pdf_resource *pres;
	    int code = pdf_begin_resource(pdev, resourceXObject, &pres);

	    if ( code < 0 )
	      return code;
	    fputs(" /Subtype /PS", file);
	    if ( level1.data != 0 )
	      { long level1_id = pdf_obj_ref(pdev);
		fprintf(file, " /Level1 %ld 0 R", level1_id);
		pdfmark_write_ps(pdev, &source);
		pdf_open_obj(pdev, level1_id);
		fputs("<<", file);
		pdfmark_write_ps(pdev, &level1);
	      }
	    else
	      pdfmark_write_ps(pdev, &source);
	    code = pdf_begin_contents(pdev);
	    if ( code < 0 )
	      return code;
	    fprintf(file, "/R%ld Do\n", pres->id);
	  }
	return 0;
}

/* PAGES pdfmark */
private int
pdfmark_PAGES(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	return pdfmark_save_pairs(pdev, pairs, count, &pdev->pages_string);
}

/* PAGE pdfmark */
private int
pdfmark_PAGE(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	return pdfmark_save_pairs(pdev, pairs, count, &pdev->page_string);
}

/* DOCINFO pdfmark */
private int
pdfmark_DOCINFO(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	FILE *file = pdev->file;
	long info_id;
	uint i;

	if ( pdev->in_contents && pdev->next_contents_id == max_contents_ids )
	  return_error(gs_error_limitcheck);
	pdf_close_contents(pdev, false);
	pdf_open_page(pdev, false);
	info_id = pdf_begin_obj(pdev);
	fputs("<<\n", file);
	for ( i = 0; i < count; i += 2 )
	  if ( !pdf_key_eq(&pairs[i], "CreationDate") &&
	       !pdf_key_eq(&pairs[i], "Producer")
	     )
	    pdfmark_write_pair(file, &pairs[i]);
	pdf_write_default_info(pdev);
	fputs(">>\n", file);
	pdf_end_obj(pdev);
	pdev->info_id = info_id;
	return 0;
}

/* DOCVIEW pdfmark */
private int
pdfmark_DOCVIEW(gx_device_pdf *pdev, const gs_param_string *pairs, uint count)
{	char dest[max_dest_string];

	if ( pdfmark_make_dest(dest, pdev, pairs, count) )
	  { gs_param_string add_dest[2];
	    static const char *skip_dest[] = { "Page", "View", 0 };

	    param_string_from_string(add_dest[0], "OpenAction");
	    param_string_from_string(add_dest[1], dest);
	    return pdfmark_save_edited_pairs(pdev, pairs, count, skip_dest,
					     add_dest, 2,
					     &pdev->catalog_string);
	  }
	else
	  return pdfmark_save_pairs(pdev, pairs, count, &pdev->catalog_string);
}
