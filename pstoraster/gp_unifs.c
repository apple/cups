/*Copyright 1993-2001 by Easy Software Products.
  Copyright 1993, 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.

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

/*$Id: gp_unifs.c,v 1.5 2001/02/15 13:34:16 mike Exp $ */
/* "Unix-like" file system platform routines for Ghostscript */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gsstruct.h"
#include "gsutil.h"		/* for string_match */
#include "stat_.h"
#include "dirent_.h"
#include <sys/param.h>		/* for MAXPATHLEN */

/* Some systems (Interactive for example) don't define MAXPATHLEN,
 * so we define it here.  (This probably should be done via a Config-Script.)
 */

#ifndef MAXPATHLEN
#  define MAXPATHLEN 1024
#endif

/* Library routines not declared in a standard header */
extern char *mktemp(P1(char *));

/* ------ File naming and accessing ------ */

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "gs_";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "/dev/null";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = ".";

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char fname[gp_file_name_sizeof],
		     const char *mode)
{				/* The -8 is for XXXXXX plus a possible final / and -. */
    int fd;
    int len = gp_file_name_sizeof - strlen(prefix) - 8;

   /*
    * MRS - Hello? TEMP is a DOS thing, TMPDIR is the UNIX thing.
    *       Also, we should default to /var/tmp, since the root
    *       partition is often small.
    */

    if (gp_getenv("TMPDIR", fname, &len) != 0)
	strcpy(fname, "/var/tmp/");
    else {
	if (strlen(fname) != 0 && fname[strlen(fname) - 1] != '/')
	    strcat(fname, "/");
    }
    strcat(fname, prefix);
    /* Prevent trailing X's in path from being converted by mktemp. */
    if (*fname != 0 && fname[strlen(fname) - 1] == 'X')
	strcat(fname, "-");
    strcat(fname, "XXXXXX");
    if ((fd = mkstemp(fname)) < 0)
      return (NULL);
    else
      return (fdopen(fd, mode));
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen(const char *fname, const char *mode)
{
    return fopen(fname, mode);
}

/* Set a file into binary or text mode. */
int
gp_setmode_binary(FILE * pfile, bool mode)
{
    return 0;			/* Noop under Unix */
}

/* ------ File enumeration ------ */

/* Thanks to Fritz Elfert (Fritz_Elfert@wue.maus.de) for */
/* the original version of the following code, and Richard Mlynarik */
/* (mly@adoc.xerox.com) for an improved version. */

typedef struct dirstack_s dirstack;
struct dirstack_s {
    dirstack *next;
    DIR *entry;
};

gs_private_st_ptrs1(st_dirstack, dirstack, "dirstack",
		    dirstack_enum_ptrs, dirstack_reloc_ptrs, next);

struct file_enum_s {
    DIR *dirp;			/* pointer to current open directory   */
    char *pattern;		/* original pattern                    */
    char *work;			/* current path                        */
    int worklen;		/* strlen (work)                       */
    dirstack *dstack;		/* directory stack                     */
    int patlen;
    int pathead;		/* how much of pattern to consider
				 *  when listing files in current directory */
    bool first_time;
    gs_memory_t *memory;
};
gs_private_st_ptrs3(st_file_enum, struct file_enum_s, "file_enum",
	  file_enum_enum_ptrs, file_enum_reloc_ptrs, pattern, work, dstack);

/* Private procedures */

/* Do a wild-card match. */
#ifdef DEBUG
private bool
wmatch(const byte * str, uint len, const byte * pstr, uint plen,
       const string_match_params * psmp)
{
    bool match = string_match(str, len, pstr, plen, psmp);

    if (gs_debug_c('e')) {
	dlputs("[e]string_match(\"");
	fwrite(str, 1, len, dstderr);
	dputs("\", \"");
	fwrite(pstr, 1, plen, dstderr);
	dprintf1("\") = %s\n", (match ? "TRUE" : "false"));
    }
    return match;
}
#define string_match wmatch
#endif

/* Search a string backward for a character. */
/* (This substitutes for strrchr, which some systems don't provide.) */
private char *
rchr(char *str, char ch, int len)
{
    register char *p = str + len;

    while (p > str)
	if (*--p == ch)
	    return p;
    return 0;
}

/* Pop a directory from the enumeration stack. */
private bool
popdir(file_enum * pfen)
{
    dirstack *d = pfen->dstack;

    if (d == 0)
	return false;
    pfen->dirp = d->entry;
    pfen->dstack = d->next;
    gs_free_object(pfen->memory, d, "gp_enumerate_files(popdir)");
    return true;
}

/* Initialize an enumeration. */
file_enum *
gp_enumerate_files_init(const char *pat, uint patlen, gs_memory_t * mem)
{
    file_enum *pfen;
    char *p;
    char *work;

    /* Reject attempts to enumerate paths longer than the */
    /* system-dependent limit. */
    if (patlen > MAXPATHLEN)
	return 0;

    /* Reject attempts to enumerate with a pattern containing zeroes. */
    {
	const char *p1;

	for (p1 = pat; p1 < pat + patlen; p1++)
	    if (*p1 == 0)
		return 0;
    }
    /* >>> Should crunch strings of repeated "/"'s in pat to a single "/"
     * >>>  to match stupid unix filesystem "conventions" */

    pfen = gs_alloc_struct(mem, file_enum, &st_file_enum,
			   "gp_enumerate_files");
    if (pfen == 0)
	return 0;

    /* pattern and work could be allocated as strings, */
    /* but it's simpler for GC and freeing to allocate them as bytes. */

    pfen->pattern =
	(char *)gs_alloc_bytes(mem, patlen + 1,
			       "gp_enumerate_files(pattern)");
    if (pfen->pattern == 0)
	return 0;
    memcpy(pfen->pattern, pat, patlen);
    pfen->pattern[patlen] = 0;

    work = (char *)gs_alloc_bytes(mem, MAXPATHLEN + 1,
				  "gp_enumerate_files(work)");
    if (work == 0)
	return 0;
    pfen->work = work;

    p = work;
    memcpy(p, pat, patlen);
    p += patlen;
    *p = 0;

    /* Remove directory specifications beyond the first wild card. */
    /* Some systems don't have strpbrk, so we code it open. */
    p = pfen->work;
    while (!(*p == '*' || *p == '?' || *p == 0))
	p++;
    while (!(*p == '/' || *p == 0))
	p++;
    if (*p == '/')
	*p = 0;
    /* Substring for first wildcard match */
    pfen->pathead = p - work;

    /* Select the next higher directory-level. */
    p = rchr(work, '/', p - work);
    if (!p) {			/* No directory specification */
	work[0] = 0;
	pfen->worklen = 0;
    } else {
	if (p == work) {	/* Root directory -- don't turn "/" into "" */
	    p++;
	}
	*p = 0;
	pfen->worklen = p - work;
    }

    pfen->memory = mem;
    pfen->dstack = 0;
    pfen->first_time = true;
    pfen->patlen = patlen;
    return pfen;
}

/* Enumerate the next file. */
uint
gp_enumerate_files_next(file_enum * pfen, char *ptr, uint maxlen)
{
    const dir_entry *de;
    char *work = pfen->work;
    int worklen = pfen->worklen;
    char *pattern = pfen->pattern;
    int pathead = pfen->pathead;
    int len;
    struct stat stbuf;

    if (pfen->first_time) {
	pfen->dirp = ((worklen == 0) ? opendir(".") : opendir(work));
	if_debug1('e', "[e]file_enum:First-Open '%s'\n", work);
	pfen->first_time = false;
	if (pfen->dirp == 0) {	/* first opendir failed */
	    gp_enumerate_files_close(pfen);
	    return ~(uint) 0;
	}
    }
  top:de = readdir(pfen->dirp);
    if (de == 0) {		/* No more entries in this directory */
	char *p;

	if_debug0('e', "[e]file_enum:Closedir\n");
	closedir(pfen->dirp);
	/* Back working directory and matching pattern up one level */
	p = rchr(work, '/', worklen);
	if (p != 0) {
	    if (p == work)
		p++;
	    *p = 0;
	    worklen = p - work;
	} else
	    worklen = 0;
	p = rchr(pattern, '/', pathead);
	if (p != 0)
	    pathead = p - pattern;
	else
	    pathead = 0;

	if (popdir(pfen)) {	/* Back up the directory tree. */
	    if_debug1('e', "[e]file_enum:Dir popped '%s'\n", work);
	    goto top;
	} else {
	    if_debug0('e', "[e]file_enum:Dirstack empty\n");
	    gp_enumerate_files_close(pfen);
	    return ~(uint) 0;
	}
    }
    /* Skip . and .. */
    len = strlen(de->d_name);
    if (len <= 2 && (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")))
	goto top;
    if (len + worklen + 1 > MAXPATHLEN)
	/* Should be an error, I suppose */
	goto top;
    if (worklen == 0) {		/* "Current" directory (evil un*x kludge) */
	memcpy(work, de->d_name, len + 1);
    } else if (worklen == 1 && work[0] == '/') {	/* Root directory */
	memcpy(work + 1, de->d_name, len + 1);
	len = len + 1;
    } else {
	work[worklen] = '/';
	memcpy(work + worklen + 1, de->d_name, len + 1);
	len = worklen + 1 + len;
    }

    /* Test for a match at this directory level */
    if (!string_match((byte *) work, len, (byte *) pattern, pathead, NULL))
	goto top;

    /* Perhaps descend into subdirectories */
    if (pathead < pfen->patlen) {
	DIR *dp;

	if (((stat(work, &stbuf) >= 0)
	     ? !stat_is_dir(stbuf)
	/* Couldn't stat it.
	 * Well, perhaps it's a directory and
	 * we'll be able to list it anyway.
	 * If it isn't or we can't, no harm done. */
	     : 0))
	    goto top;

	if (pfen->patlen == pathead + 1) {	/* Listing "foo/?/" -- return this entry */
	    /* if it's a directory. */
	    if (!stat_is_dir(stbuf)) {	/* Do directoryp test the hard way */
		dp = opendir(work);
		if (!dp)
		    goto top;
		closedir(dp);
	    }
	    work[len++] = '/';
	    goto winner;
	}
	/* >>> Should optimise the case in which the next level */
	/* >>> of directory has no wildcards. */
	dp = opendir(work);
#ifdef DEBUG
	{
	    char save_end = pattern[pathead];

	    pattern[pathead] = 0;
	    if_debug2('e', "[e]file_enum:fname='%s', p='%s'\n",
		      work, pattern);
	    pattern[pathead] = save_end;
	}
#endif /* DEBUG */
	if (!dp)
	    /* Can't list this one */
	    goto top;
	else {			/* Advance to the next directory-delimiter */
	    /* in pattern */
	    char *p;
	    dirstack *d;

	    for (p = pattern + pathead + 1;; p++) {
		if (*p == 0) {	/* No more subdirectories to match */
		    pathead = pfen->patlen;
		    break;
		} else if (*p == '/') {
		    pathead = p - pattern;
		    break;
		}
	    }

	    /* Push a directory onto the enumeration stack. */
	    d = gs_alloc_struct(pfen->memory, dirstack,
				&st_dirstack,
				"gp_enumerate_files(pushdir)");
	    if (d != 0) {
		d->next = pfen->dstack;
		d->entry = pfen->dirp;
		pfen->dstack = d;
	    } else
		DO_NOTHING;	/* >>> e_VMerror!!! */

	    if_debug1('e', "[e]file_enum:Dir pushed '%s'\n",
		      work);
	    worklen = len;
	    pfen->dirp = dp;
	    goto top;
	}
    }
  winner:
    /* We have a winner! */
    pfen->worklen = worklen;
    pfen->pathead = pathead;
    memcpy(ptr, work, len);
    return len;
}

/* Clean up the file enumeration. */
void
gp_enumerate_files_close(file_enum * pfen)
{
    gs_memory_t *mem = pfen->memory;

    if_debug0('e', "[e]file_enum:Cleanup\n");
    while (popdir(pfen))	/* clear directory stack */
	DO_NOTHING;
    gs_free_object(mem, (byte *) pfen->work,
		   "gp_enumerate_close(work)");
    gs_free_object(mem, (byte *) pfen->pattern,
		   "gp_enumerate_files_close(pattern)");
    gs_free_object(mem, pfen, "gp_enumerate_files_close");
}

/* Test-cases:
   (../?*r*?/?*.ps) {==} 100 string filenameforall
   (../?*r*?/?*.ps*) {==} 100 string filenameforall
   (../?*r*?/) {==} 100 string filenameforall
   (/t*?/?*.ps) {==} 100 string filenameforall
 */
