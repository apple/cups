/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved. */

/* dsc.c */
/* Parse DSC comments from a PostScript file. */
#include "stdpre.h"
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#ifndef SEEK_SET
#  define SEEK_SET 0
#endif

/* Define the maximum length of a line in a DSC-conforming file. */
/* This is 255 characters + 2 for newline + 1 for null. */
#define line_size 258

/* Test whether a line begins with %%. */
#define is_dsc_comment(line) ((line)[0] == '%' && (line)[1] == '%')

/* Test whether a string begins with a given prefix. */
#define has_prefix(str, pre) !strncmp(str, pre, strlen(pre))
/* Faster test if prefix is a literal string. */
#define has_lit_prefix(str, litpre) !strncmp(str, litpre, sizeof(litpre) - 1)
/* Test whether a line is a specific DSC comment. */
#define has_dsc_prefix(line, litpre)\
  (is_dsc_comment(line) && has_lit_prefix(line, litpre))

/* Define a bounding box structure. */
#define LLX 0
#define LLY 1
#define URX 2
#define URY 3

/* ------ Internal routines ------ */

/* Allocate a block with malloc, exiting on failure. */
private void *
dsc_malloc(size_t size)
{	void *blk = malloc(size);
	if ( blk == 0 )
	  {	fprintf(stderr, "Unable to allocate object; giving up.\n");
		exit(255);
	  }
	return blk;
}

/* Copy a given amount of data from one file to another (to != NULL) or */
/* skip a given amount of data in a file without using fseek (to == NULL). */
#define fcpy_size 5000
private void
fcpy(FILE *to, FILE *from, long len)
{	char buf[fcpy_size];
	size_t count;
	for ( ; len > 0; len -= count )
	  {	count = (len > fcpy_size ? fcpy_size : len);
		fread(buf, sizeof(char), count, from);
		if ( to != NULL )
		  fwrite(buf, sizeof(char), count, to);
	  }
}
#undef fcpy_size

/*
 * Copy (or skip) a %%BeginBinary/%%EndBinary section.
 * Return the number of bytes copied or skipped.
 */
private long
copy_binary(FILE *to, FILE *from, const char *line)
{	long count;
	if ( sscanf(line + 14, "%ld", &count) == 1 )
	  fcpy(to, from, count);
	else
	  count = 0;
	return count;
}

/*
 * Copy (or skip) a %%BeginData/%%EndData section.
 * Return the number of bytes copied or skipped.
 */
private long
copy_data(FILE *to, FILE *from, const char *line)
{	long count, amount = 0;
	char str[line_size];
	str[0] = '\0';
	if ( sscanf(line + 12, "%ld %*s %s", &count, str) >= 1 )
	  {	if ( !strcmp(str, "Lines") )
		  for ( ; count > 0; --count )
		    {	if ( !fgets(str, line_size, from) )
			  break;
			amount += strlen(str);
			if ( to != NULL )
			  fputs(str, to);
		    }
		else
		  {	amount = count;
			fcpy(to, from, count);
		  }
	  }
	return amount;
}

/*
 * Read the next line from the input.  Skip over embedded data, and also
 * skip sections (such as procsets, fonts, and included documents) that
 * are not part of the main document flow.
 * Return a pointer to the input line, or NULL if we reached EOF.
 */
private void skip_region(P4(char *, FILE *, long *, const char *));
private char *
next_line(char *line, FILE *in, long *pstart, long *plen)
{	if ( pstart )
	  *pstart = ftell(in);
	*plen = 0;
	for ( ; ; )
	  {	if ( !fgets(line, line_size, in) )	/* EOF */
		  {	line[0] = 0;
			return 0;
		  }
		*plen += strlen(line);
		if ( !has_dsc_comment(line, "Begin") )
		  break;
		else if ( has_lit_prefix(line+7, "Binary:") )
		  {	*plen += copy_binary(NULL, in, line);
			skip_region(line, in, plen, "EndBinary");
		  }
		else if ( has_lit_prefix(line+7, "Data:") )
		  {	*plen += copy_data(NULL, in, line);
			skip_region(line, in, plen, "EndData");
		  }
		else if ( has_lit_prefix(line+7, "Feature:") )
		  skip_region(line, in, plen, "EndFeature");
		else if ( has_lit_prefix(line+7, "File:") )
		  skip_region(line, in, plen, "EndFile");
		else if ( has_lit_prefix(line+7, "Font:") )
		  skip_region(line, in, plen, "EndFont");
		else if ( has_lit_prefix(line+7, "ProcSet:") )
		  skip_region(line, in, plen, "EndProcSet");
		else if ( has_lit_prefix(line+7, "Resource:") )
		  skip_region(line, in, plen, "EndResource");
	  }
	return line;
}
/*
 * Skip a region of the PostScript file up to a given %%End comment.
 */
private void
skip_region(char *line, FILE *in, long *plen, const char *end_comment)
{	do
	  {	long len;
		if ( next_line(line, in, NULL, &len) == NULL )
		  break;
		*plen += len;
	  }
	while ( !has_dsc_prefix(line, end_comment) );
}

/*
 * Scan a bounding box argument.  Return 1 iff we found one.
 */
private int
scan_bbox(const char *line, int bbox[4])
{	float fx0, fy0, fx1, fy1;
	if ( sscanf(line, "%d %d %d %d", &bbox[LLX], &bbox[LLY],
		    &bbox[URX], &bbox[URY]) == 4
	   )
	  return 1;
	if ( sscanf(line, "%f %f %f %f", &fx0, &fy0, &fx1, &fy1) != 4 )
	  return 0;
	bbox[LLX] = (int)floor(fx0);
	bbox[LLY] = (int)floor(fy0);
	bbox[URX] = (int)ceil(fx1);
	bbox[URY] = (int)ceil(fy1);
	return 1;
}

/*
 * Scan a text argument, recognizing escapes if it is a parenthesized string.
 * If the string is not parenthesized then if rest = 1, take the rest of
 * the line as the argument; if rest = 0, only take up to the next whitespace.
 */
#define scan_text_arg(line, endp) scan_text(line, endp, 0)
#define scan_line_arg(line, endp) scan_text(line, endp, 1)
private const char *
scan_text(const char *line, const char **endp, int rest)
{	char buf[line_size];
	const char *lp = line;
	char *bp = buf;
	char *arg;

	while ( *lp == ' ' || *lp == '\t' )
	  lp++;
	if ( *lp == '(' )
	  {	int level = 1;
		lp++;
		for ( ; ; )
		  switch ( *bp++ = *lp++ )
		    {
		    case '\\':
			switch ( *lp++ )
			{
			case 'n':
				bp[-1] = '\n'; break;
			case 'r':
				bp[-1] = '\r'; break;
			case 't':
				bp[-1] = '\t'; break;
			case 'b':
				bp[-1] = '\b'; break;
			case 'f':
				bp[-1] = '\f'; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			  {	int c = lp[-1] - '0';
				int d = *lp;
				if ( d >= '0' && d <= '7' )
				{	c = (c << 3) + d - '0';
					d = *++lp;
					if ( d >= '0' && d <= '7' )
					{	c = (c << 3) + d - '0';
						lp++;
					}
				}
				bp[-1] = c;
				break;
			  }
			case 0:		/* unexpected EOF */
				lp--;
				goto out;
			default:
				bp[-1] = lp[-1];
			}
			break;
		    case '(':
			++level;
			break;
		    case ')':
			if ( --level )
			  break;
		    case 0:
			goto out;
		    default:
			;
		    }
 out:		--bp;
	  }
	else
	  {	/* Not quoted. */
		while ( !(*lp == 0 || *lp == '\n' ||
			  (!rest && (*lp == ' ' || *lp == '\t')))
		      )
		  *bp++ = *lp++;
		if ( bp == buf )	/* empty */
		  return NULL;
	  }
	*bp = 0;
	if ( endp )
	  *endp = lp;
	arg = dsc_malloc(bp - buf + 1);
	strcpy(arg, buf);
	return arg;
}

/* ------ Public routines ------ */

/*
 * Copy a section of a DSC-conforming PostScript file.
 * Detect %%(Begin|End)(Binary|Data) comments and copy the intervening
 * data as binary if necessary.  If a sentinel is specified,
 * stop copying when we reach a line that begins with the sentinel.
 * If start < 0, don't seek before copying.
 */
private int dsc_copy_section(P6(FILE *from, FILE *to, long start, long end,
				char *line, const char *sentinel));
void
dsc_copy(FILE *from, FILE *to, long start, long end)
{	char line[line_size];
	dsc_copy_section(from, to, start, end, line, NULL);
}
char *
dsc_copy_until(FILE *from, FILE *to, long start, long end,
	       const char *sentinel)
{	char line[line_size];
	int found = dsc_copy_section(from, to, start, end, line, sentinel);
	if ( found )
	  {	char *ret = dsc_malloc(strlen(line) + 1);
		strcpy(ret, line);
		return ret;
	  }
	return NULL;
}
private int
dsc_copy_section(FILE *from, FILE *to, long start, long end,
		 char *line, const char *sentinel)
{	int sent_len = (sentinel == 0 ? 0 : strlen(sentinel));

	if ( start >= 0 )
	  fseek(from, start, SEEK_SET);
	while ( ftell(from) < end )
	  {	long count;
		fgets(line, line_size, from);
		if ( sent_len )
		  {	if ( !strncmp(line, sentinel, sent_len) )
			  return 1;
		  }
		fputs(line, to);
		if ( !has_dsc_prefix(line, "Begin") )
		  ;
		else if ( has_lit_prefix(line + 7, "Binary:") )
		  copy_binary(to, from, line);
		else if ( has_lit_prefix(line + 7, "Data:") )
		  copy_data(to, from, line);
	  }
	return 0;
}
