/* Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* gp_os2.c */
/* Common platform-specific routines for OS/2 and MS-DOS */
/* compiled with GCC/EMX */

#define INCL_DOS
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_SPLERRORS
#define INCL_BASE
#define INCL_ERRORS
#define INCL_WIN
#include <os2.h>

#include "stdio_.h"
#include "string_.h"
#include <fcntl.h>

#ifdef __IBMC__
#define popen fopen	/* doesn't support popen */
#define pclose fclose	/* doesn't support pclose */
#else
#include <dos.h>
#endif
/* Define the regs union tag for short registers. */
#  define rshort x
#define intdos(a,b) _int86(0x21, a, b)

#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gsexit.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gp.h"
#include "gsutil.h"
#include "stdlib.h"		/* need _osmode, exit */
#include "time_.h"
#include <time.h>		/* should this be in time_.h? */
#include "gdevpm.h"
#ifdef __EMX__
#include <sys/emxload.h>
#endif

#if defined(__DLL__) && defined( __EMX__)
/* This isn't provided in any of the libraries */
/* We set this to the process environment in gp_init */
char *fake_environ[3] = {"", NULL, NULL};
char **environ = fake_environ;
char **_environ = fake_environ;
HWND hwndtext = (HWND)NULL;
#endif

#ifdef __DLL__
/* use longjmp instead of exit when using DLL */
#include <setjmp.h>
extern jmp_buf gsdll_env;
#endif

#ifdef __DLL__
#define isos2 TRUE
#else
#define isos2 (_osmode == OS2_MODE)
#endif
char pm_prntmp[256];	/* filename of printer spool temporary file */


/* ------ Miscellaneous ------ */

/* Get the string corresponding to an OS error number. */
/* All reasonable compilers support it. */
const char *
gp_strerror(int errnum)
{	return strerror(errnum);
}

/* use Unix version for date and time */
/* ------ Date and time ------ */

/* Read the current time (in seconds since Jan. 1, 1970) */
/* and fraction (in nanoseconds since midnight). */
void
gp_get_realtime(long *pdt)
{	struct timeval tp;
	struct timezone tzp;

	if ( gettimeofday(&tp, &tzp) == -1 )
	   {	lprintf("Ghostscript: gettimeofday failed!\n");
		gs_exit(1);
	   }

	/* tp.tv_sec is #secs since Jan 1, 1970 */
	pdt[0] = tp.tv_sec;
	pdt[1] = tp.tv_usec * 1000;

#ifdef DEBUG_CLOCK
	printf("tp.tv_sec = %d  tp.tv_usec = %d  pdt[0] = %ld  pdt[1] = %ld\n",
		tp.tv_sec, tp.tv_usec, pdt[0], pdt[1]);
#endif
}

/* Read the current user CPU time (in seconds) */
/* and fraction (in nanoseconds).  */
void
gp_get_usertime(long *pdt)
{	gp_get_realtime(pdt);	/* Use an approximation for now.  */
}


/* ------ Console management ------ */

/* Answer whether a given file is the console (input or output). */
/* This is not a standard gp procedure, */
/* but the MS Windows configuration needs it, */
/* and other MS-DOS configurations might need it someday. */
/* Don't know if it is needed for OS/2. */
bool
gp_file_is_console(FILE *f)
{
#ifndef __DLL__
    if (!isos2) {
	union REGS regs;
	if ( f == NULL )
		return false;
	regs.h.ah = 0x44;	/* ioctl */
	regs.h.al = 0;		/* get device info */
	regs.rshort.bx = fileno(f);
	intdos(&regs, &regs);
	return ((regs.h.dl & 0x80) != 0 && (regs.h.dl & 3) != 0);
    }
#endif
    if ( (f==stdin) || (f==stdout) || (f==stderr) )
	return true;
    return false;
}

/* ------ File naming and accessing ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ';';

/* Define the default scratch file name prefix. */
const char gp_scratch_file_name_prefix[] = "gs";

/* Define the name of the null output file. */
const char gp_null_file_name[] = "nul";

/* Define the name that designates the current directory. */
const char gp_current_directory_name[] = ".";

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char gp_fmode_binary_suffix[] = "b";
/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";

/* Answer whether a file name contains a directory/device specification, */
/* i.e. is absolute (not directory- or device-relative). */
bool
gp_file_name_is_absolute(const char *fname, uint len)
{	/* A file name is absolute if it contains a drive specification */
	/* (second character is a :) or if it start with 0 or more .s */
	/* followed by a / or \. */
	if ( len >= 2 && fname[1] == ':' )
	  return true;
	while ( len && *fname == '.' )
	  ++fname, --len;
	return (len && (*fname == '/' || *fname == '\\'));
}

/* Answer the string to be used for combining a directory/device prefix */
/* with a base file name.  The file name is known to not be absolute. */
const char *
gp_file_name_concat_string(const char *prefix, uint plen,
  const char *fname, uint len)
{	if ( plen > 0 )
	  switch ( prefix[plen - 1] )
	   {	case ':': case '/': case '\\': return "";
	   };
	return "\\";
}


/* ------ File enumeration ------ */


struct file_enum_s {
	FILEFINDBUF3 findbuf;
	HDIR hdir;
	char *pattern;
	int patlen;			/* orig pattern length */
	int pat_size;			/* allocate space for pattern */
	int head_size;			/* pattern length through last */
					/* :, / or \ */
	int first_time;
	gs_memory_t *memory;
};
gs_private_st_ptrs1(st_file_enum, struct file_enum_s, "file_enum",
  file_enum_enum_ptrs, file_enum_reloc_ptrs, pattern);

/* Initialize an enumeration.  may NEED WORK ON HANDLING * ? \. */
file_enum *
gp_enumerate_files_init(const char *pat, uint patlen, gs_memory_t *mem)
{	file_enum *pfen = gs_alloc_struct(mem, file_enum, &st_file_enum, "gp_enumerate_files");
	int pat_size = 2 * patlen + 1;
	char *pattern;
	int hsize = 0;
	int i;
	if ( pfen == 0 ) return 0;

	/* pattern could be allocated as a string, */
	/* but it's simpler for GC and freeing to allocate it as bytes. */

	pattern = (char *)gs_alloc_bytes(mem, pat_size,
					 "gp_enumerate_files(pattern)");
	if ( pattern == 0 ) return 0;
	memcpy(pattern, pat, patlen);
	/* find directory name = header */
	for ( i = 0; i < patlen; i++ )
	{	switch ( pat[i] )
		{
		case '\\':
			if ( i + 1 < patlen && pat[i + 1] == '\\' )
				i++;
			/* falls through */
		case ':':
		case '/':
			hsize = i + 1;
		}
	}
	pattern[patlen] = 0;
	pfen->pattern = pattern;
	pfen->patlen = patlen;
	pfen->pat_size = pat_size;
	pfen->head_size = hsize;
	pfen->memory = mem;
	pfen->first_time = 1;
	pfen->hdir = HDIR_CREATE;
	return pfen;
}

/* Enumerate the next file. */
uint
gp_enumerate_files_next(file_enum *pfen, char *ptr, uint maxlen)
{
    APIRET rc;
    ULONG cFilenames = 1;
    if (!isos2) {
	/* CAN'T DO IT SO JUST RETURN THE PATTERN. */
	if ( pfen->first_time )
	{	char *pattern = pfen->pattern;
		uint len = strlen(pattern);
		pfen->first_time = 0;
		if ( len > maxlen )
			return maxlen + 1;
		strcpy(ptr, pattern);
		return len;
	}
	return -1;
    }

    /* else OS/2 */
    if ( pfen->first_time ) {
	rc = DosFindFirst(pfen->pattern, &pfen->hdir, FILE_NORMAL,
		&pfen->findbuf, sizeof(pfen->findbuf), 
		&cFilenames, FIL_STANDARD);
	pfen->first_time = 0;
    }
    else {
	rc = DosFindNext(pfen->hdir, &pfen->findbuf, sizeof(pfen->findbuf),
		&cFilenames);
    }
    if (rc)
	return -1;
    
    if (pfen->head_size + pfen->findbuf.cchName < maxlen) {
        memcpy(ptr, pfen->pattern, pfen->head_size);
	strcpy(ptr + pfen->head_size, pfen->findbuf.achName);
	return pfen->head_size + pfen->findbuf.cchName;
    }

    if (pfen->head_size >= maxlen)
	return 0;	/* no hope at all */

    memcpy(ptr, pfen->pattern, pfen->head_size);
    strncpy(ptr + pfen->head_size, pfen->findbuf.achName, 
	    maxlen - pfen->head_size - 1);
    return maxlen;
}

/* Clean up the file enumeration. */
void
gp_enumerate_files_close(file_enum *pfen)
{
	gs_memory_t *mem = pfen->memory;
	if (isos2)
		DosFindClose(pfen->hdir);
	gs_free_object(mem, pfen->pattern,
		       "gp_enumerate_files_close(pattern)");
	gs_free_object(mem, pfen, "gp_enumerate_files_close");
}

/*************************************************************/
/* from gp_iwatc.c and gp_itbc.c */

/* Intel processor, EMX/GCC specific routines for Ghostscript */
#include <signal.h>
#include "stat_.h"
#include "string_.h"

/* Library routines not declared in a standard header */
/* extern char *getenv(P1(const char *)); */

/* Forward declarations */
private void handle_FPE(P1(int));

/* Do platform-dependent initialization. */
void
gp_init(void)
{
#if defined(__DLL__) && defined(__EMX__)
	PTIB pptib;
	PPIB pppib;
	int i;
	char *p;
	/* get environment of EXE */
	DosGetInfoBlocks(&pptib, &pppib);
	for (i=0, p=pppib->pib_pchenv; *p; p+=strlen(p)+1)
	    i++;
	_environ = environ = (char **)malloc((i+2) * sizeof(char *));
 	for (i=0, p=pppib->pib_pchenv; *p; p+=strlen(p)+1) {
	    environ[i] = p;
	    i++;
	}
	environ[i] = p;
	i++;
	environ[i] = NULL;
#endif

	/* keep gsos2.exe in memory for number of minutes specified in */
	/* environment variable GS_LOAD */
#ifdef __EMX__
	_emxload_env("GS_LOAD");
#endif
	/* Set up the handler for numeric exceptions. */
	signal(SIGFPE, handle_FPE);
	gp_init_console();
}


/* Trap numeric exceptions.  Someday we will do something */
/* more appropriate with these. */
private void
handle_FPE(int sig)
{	eprintf("Numeric exception:\n");
	exit(1);
}

extern int gs_exit_status;
/* Do platform-dependent cleanup. */
void
gp_exit(int exit_status, int code)
{
#ifndef __DLL__
	if (exit_status && (_osmode == OS2_MODE))
		DosSleep(2000);
#endif
#if defined(__DLL__) && defined(__EMX__)
	if (environ != fake_environ) {
	    free(environ);
	    environ = _environ = fake_environ;
	}
#endif
}

/* Exit the program. */
void
gp_do_exit(int exit_status)
{
#if defined(__DLL__)
	/* Use longjmp since exit would terminate caller */
	/* setjmp code will check gs_exit_status */
	longjmp(gsdll_env, gs_exit_status);
#else
	exit(exit_status);
#endif
}

/* ------ Printer accessing ------ */
private int pm_find_queue(char *queue_name, char *driver_name);
private int is_os2_spool(const char *queue);
private int pm_spool(char *filename, const char *queue);
  
/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
void
gp_set_printer_binary(int prnfno, int binary)
{
#ifndef __IBMC__
	union REGS regs;
	regs.h.ah = 0x44;	/* ioctl */
	regs.h.al = 0;		/* get device info */
	regs.rshort.bx = prnfno;
	intdos(&regs, &regs);
	if ( ((regs.rshort.flags)&1) != 0 || !(regs.h.dl & 0x80) )
		return;		/* error, or not a device */
	if ( binary )
		regs.h.dl |= 0x20;	/* binary (no ^Z intervention) */
	else
		regs.h.dl &= ~0x20;	/* text */
	regs.h.dh = 0;
	regs.h.ah = 0x44;	/* ioctl */
	regs.h.al = 1;		/* set device info */
	intdos(&regs, &regs);
#endif
}

/* Open a connection to a printer.  A null file name means use the */
/* standard printer connected to the machine, if any. */
/* Return NULL if the connection could not be opened. */
/* filename can be one of the following values
 *   ""                Spool in default queue
 *   "\\spool\queue"   Spool in "queue"
 *   "|command"        open an output pipe using popen()
 *   "filename"        open filename using fopen()
 *   "port"            open port using fopen()
 */
FILE *
gp_open_printer(char *fname, int binary_mode)
{	FILE *pfile;
	if ( (strlen(fname) == 0) || is_os2_spool(fname) ) {
	    if (isos2) {
		/* default or spool */
		if (pm_spool(NULL, fname))    /* check if spool queue valid */
		    return NULL;
		pfile = gp_open_scratch_file(gp_scratch_file_name_prefix, 
			    pm_prntmp, (binary_mode ? "wb" : "w"));
	    }
	    else
		pfile = fopen("PRN", (binary_mode ? "wb" : "w"));
	}
	else if ( (isos2) && (fname[0] == '|') )
	    /* pipe */
	    pfile = popen(fname+1, (binary_mode ? "wb" : "w"));
	else
	    /* normal file or port */
	    pfile = fopen(fname, (binary_mode ? "wb" : "w"));

	if ( pfile == (FILE *)NULL )
		return (FILE *)NULL;
	if (!isos2)
	    gp_set_printer_binary(fileno(pfile), binary_mode);
	return pfile;
}

/* Close the connection to the printer. */
void
gp_close_printer(FILE *pfile, const char *fname)
{	
	if ( isos2 && (fname[0] == '|') )
		pclose(pfile);
	else
		fclose(pfile);

	if ( (strlen(fname) == 0) || is_os2_spool(fname) ) {
	    /* spool temporary file */
	    pm_spool(pm_prntmp, fname);
	    unlink(pm_prntmp);
	}
}

/* ------ Printer Spooling ------ */
#ifndef NERR_BufTooSmall
#define NERR_BufTooSmall 2123	/* For SplEnumQueue */
#endif

/* If queue_name is NULL, list available queues */
/* If strlen(queue_name)==0, return default queue and driver name */
/* If queue_name supplied, return driver_name */
/* returns 0 if OK, non-zero for error */
private int
pm_find_queue(char *queue_name, char *driver_name)
{
    SPLERR splerr;
    USHORT jobCount;
    ULONG  cbBuf;
    ULONG  cTotal;
    ULONG  cReturned;
    ULONG  cbNeeded;
    ULONG  ulLevel;
    ULONG  i;
    PSZ    pszComputerName;
    PBYTE  pBuf;
    PPRQINFO3 prq;
 
    ulLevel = 3L;
    pszComputerName = (PSZ)NULL ;
    splerr = SplEnumQueue(pszComputerName, ulLevel, pBuf, 0L, /* cbBuf */
                          &cReturned, &cTotal,
                          &cbNeeded, NULL);
    if ( splerr == ERROR_MORE_DATA || splerr == NERR_BufTooSmall ) {
       if (!DosAllocMem( (PVOID)&pBuf, cbNeeded,
                         PAG_READ|PAG_WRITE|PAG_COMMIT) ) {
          cbBuf = cbNeeded ;
          splerr = SplEnumQueue(pszComputerName, ulLevel, pBuf, cbBuf,
                                  &cReturned, &cTotal,
                                  &cbNeeded, NULL);
          if (splerr == NO_ERROR) {
             /* Set pointer to point to the beginning of the buffer.           */
             prq = (PPRQINFO3)pBuf ;
 
             /* cReturned has the count of the number of PRQINFO3 structures.  */
             for (i=0;i < cReturned ; i++) {
		if (queue_name) {
		    /* find queue name and return driver name */
		    if (strlen(queue_name)==0) {  /* use default queue */
		        if ( prq->fsType & PRQ3_TYPE_APPDEFAULT )
			    strcpy(queue_name, prq->pszName);
		    }
		    if (strcmp(prq->pszName, queue_name) == 0) {
			char *p;
			for (p=prq->pszDriverName; *p && (*p!='.'); p++)
			    /* do nothing */ ;
			*p = '\0';  /* truncate at '.' */
			if (driver_name != NULL)
			    strcpy(driver_name, prq->pszDriverName);
			DosFreeMem((PVOID)pBuf) ;
			return 0;
		    }
		}
		else {
		    /* list queue details */
		    if ( prq->fsType & PRQ3_TYPE_APPDEFAULT )
		        fprintf(stdout, "  %s  (DEFAULT)\n", prq->pszName) ;
		    else
		        fprintf(stdout, "  %s\n", prq->pszName) ;
		}
                prq++;
             }/*endfor cReturned */
          }
          DosFreeMem((PVOID)pBuf) ;
       }
    } /* end if Q level given */
    else {
       /* If we are here we had a bad error code. Print it and some other info.*/
       fprintf(stdout, "SplEnumQueue Error=%ld, Total=%ld, Returned=%ld, Needed=%ld\n",
               splerr, cTotal, cReturned, cbNeeded) ;
    }
    if (splerr)
        return splerr;
    if (queue_name)
        return -1;
    return 0;
}


/* return TRUE if queue looks like a valid OS/2 queue name */
private int
is_os2_spool(const char *queue)
{
char *prefix = "\\\\spool\\";  /* 8 characters long */
int i;
    for (i=0; i<8; i++) {
	if (prefix[i] == '\\') {
	    if ((*queue != '\\') && (*queue != '/'))
	        return FALSE;
	}
	else if (tolower(*queue) != prefix[i])
	    return FALSE;
	queue++;
    }
    return TRUE;
}

#define PRINT_BUF_SIZE 16384

/* Spool file to queue */
/* return 0 if successful, non-zero if error */
/* if filename is NULL, return 0 if spool queue is valid, non-zero if error */
private int
pm_spool(char *filename, const char *queue)
{
HSPL hspl;
PDEVOPENSTRUC pdata;
PSZ  pszToken = "*";
ULONG jobid;
BOOL rc;
char queue_name[256];
char driver_name[256];
char *buffer;
FILE *f;
int count;
    if (strlen(queue)!= 0) {
	/* queue specified */
	strcpy(queue_name, queue+8);  /* skip over \\spool\ */
    }
    else {
	/* get default queue */
        queue_name[0] = '\0';
    }
    if (pm_find_queue(queue_name, driver_name)) {
	/* error, list valid queue names */
	fprintf(stdout, "Invalid queue name.  Use one of:\n");
	pm_find_queue(NULL,NULL);
	return 1;
    }
    if (!filename)
	return 0;	/* we were only asked to check the queue */


    if ((buffer = malloc(PRINT_BUF_SIZE)) == (char *)NULL) {
	fprintf(stdout, "Out of memory in pm_spool\n");
	return 1;
    }
    if ((f = fopen(filename, "rb")) == (FILE *)NULL) {
	free(buffer);
	fprintf(stdout, "Can't open temporary file %s\n", filename);
	return 1;
    }

    /* Allocate memory for pdata */
    if ( !DosAllocMem( (PVOID)&pdata,sizeof( DEVOPENSTRUC ),
         (PAG_READ|PAG_WRITE|PAG_COMMIT ) ) ) {
        /* Initialize elements of pdata */
        pdata->pszLogAddress      = queue_name;
        pdata->pszDriverName      = driver_name;
        pdata->pdriv              = NULL;
        pdata->pszDataType        = "PM_Q_RAW";
        pdata->pszComment         = "Ghostscript";
        pdata->pszQueueProcName   = NULL;
        pdata->pszQueueProcParams = NULL;
        pdata->pszSpoolerParams   = NULL;
        pdata->pszNetworkParams   = NULL;
 
        hspl = SplQmOpen( pszToken,4L,( PQMOPENDATA )pdata );
        if ( hspl == SPL_ERROR ) {
            fprintf(stdout, "SplQmOpen failed.\n");
	    DosFreeMem((PVOID)pdata);
	    free(buffer);
	    fclose(f);
	    return 1;	/* failed */
        }

	rc = SplQmStartDoc(hspl, "Ghostscript");
	if (!rc) {
	    fprintf(stdout, "SplQmStartDoc failed.\n");
	    DosFreeMem((PVOID)pdata);
	    free(buffer);
	    fclose(f);
	    return 1;
	}
	
	/* loop, copying file to queue */
	while (rc && (count = fread(buffer, 1, PRINT_BUF_SIZE, f)) != 0 ) {
	    rc = SplQmWrite(hspl, count, buffer);
	    if (!rc)
		fprintf(stdout, "SplQmWrite failed.\n");
  	}
	free(buffer);
	fclose(f);

	if (!rc) {
	  fprintf(stdout, "Aborting Spooling.\n");
          SplQmAbort(hspl);
        }
	else {
	    SplQmEndDoc(hspl);
	    rc = SplQmClose(hspl);
	    if (!rc)
		fprintf(stdout, "SplQmClose failed.\n");
	}
    }
    else
	rc = 0;	/* no memory */
    return !rc;
}

/* ------ File naming and accessing ------ */

/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char *fname, const char *mode)
{	char *temp;
#ifdef __IBMC__
	char *tname;
	*fname = 0;
	temp = getenv("TEMP");
	tname = _tempnam(temp, (char *)prefix);
	if (tname) {
	    strcpy(fname, tname);
	    free(tname);
	}
#else
	if ( (temp = getenv("TEMP")) == NULL )
		*fname = 0;
	else
	{	char last = '\\';
		strcpy(fname, temp);
		/* Prevent X's in path from being converted by mktemp. */
		for ( temp = fname; *temp; temp++ )
			*temp = last = tolower(*temp);
		switch ( last )
		{
		default:
			strcat(fname, "\\");
		case ':': case '\\':
			;
		}
	}
	strcat(fname, prefix);
	strcat(fname, "XXXXXX");
	mktemp(fname);
#endif
	return fopen(fname, mode);
}

/* Open a file with the given name, as a stream of uninterpreted bytes. */
FILE *
gp_fopen(const char *fname, const char *mode)
{	return fopen(fname, mode);
}

#ifdef __DLL__

/* The DLL version must not be allowed direct access to stdin and stdout */
/* Instead these are redirected to the gsdll_callback */
#include "gsdll.h"
#include <stdarg.h>

/* for redirecting stdin/out/err */
#include "stream.h"
#include "gxiodev.h"			/* must come after stream.h */


/* ====== Substitute for stdio ====== */

/* this code has been derived from gp_mswin.c */

/* Forward references */
private stream_proc_process(pm_std_read_process);
private stream_proc_process(pm_std_write_process);

/* Use a pseudo IODevice to get pm_stdio_init called at the right time. */
/* This is bad architecture; we'll fix it later. */
private iodev_proc_init(pm_stdio_init);
gx_io_device gs_iodev_wstdio = {
	"wstdio", "Special",
	{ pm_stdio_init, iodev_no_open_device,
	  iodev_no_open_file, iodev_no_fopen, iodev_no_fclose,
	  iodev_no_delete_file, iodev_no_rename_file,
	  iodev_no_file_status, iodev_no_enumerate_files
	}
};

/* Discard the contents of the buffer when reading. */
void
pm_std_read_reset(stream *s)
{	s_std_read_reset(s);
	s->end_status = 0;
}

/* Define alternate 'open' routines for our stdin/out/err streams. */

extern int iodev_stdin_open(P4(gx_io_device *, const char *, stream **,
			       gs_memory_t *));
private int
pm_stdin_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdin_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.reset = pm_std_read_reset;
	s->procs.process = pm_std_read_process;
	s->file = NULL;
	return 0;
}

extern int iodev_stdout_open(P4(gx_io_device *, const char *, stream **,
				gs_memory_t *));
private int
pm_stdout_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stdout_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.process = pm_std_write_process;
	s->file = NULL;
	return 0;
}

extern int iodev_stderr_open(P4(gx_io_device *, const char *, stream **,
				gs_memory_t *));
private int
pm_stderr_open(gx_io_device *iodev, const char *access, stream **ps,
  gs_memory_t *mem)
{	int code = iodev_stderr_open(iodev, access, ps, mem);
	stream *s = *ps;
	if ( code != 1 )
	  return code;
	s->procs.process = pm_std_write_process;
	s->file = NULL;
	return 0;
}

/* Patch stdin/out/err to use our windows. */
private int
pm_stdio_init(gx_io_device *iodev, gs_memory_t *mem)
{
	/* If stdxxx is the console, replace the 'open' routines, */
	/* which haven't gotten called yet. */

	if ( gp_file_is_console(gs_stdin) )
	  gs_findiodevice((const byte *)"%stdin", 6)->procs.open_device =
	    pm_stdin_open;

	if ( gp_file_is_console(gs_stdout) )
	  gs_findiodevice((const byte *)"%stdout", 7)->procs.open_device =
	    pm_stdout_open;

	if ( gp_file_is_console(gs_stderr) )
	  gs_findiodevice((const byte *)"%stderr", 7)->procs.open_device =
	    pm_stderr_open;

	return 0;
}


/* We should really use a private buffer for line reading, */
/* because we can't predict the size of the supplied input area.... */
private int
pm_std_read_process(stream_state *st, stream_cursor_read *ignore_pr,
  stream_cursor_write *pw, bool last)
{
	int count = pw->limit - pw->ptr;

	if ( count == 0 ) 		/* empty buffer */
		return 1;

        /* callback to get more input */
	count = (*pgsdll_callback)(GSDLL_STDIN, pw->ptr+1, count);
	if (count == 0) {
	    /* EOF */
	    /* what should we do? */
	    return EOFC;
	}

	pw->ptr += count;
	return 1;
}

private int
pm_std_write_process(stream_state *st, stream_cursor_read *pr,
  stream_cursor_write *ignore_pw, bool last)
{	uint count = pr->limit - pr->ptr;
	(*pgsdll_callback)(GSDLL_STDOUT, (char *)(pr->ptr + 1), count);
	pr->ptr = pr->limit;
	return 0;
}

/* This is used instead of the stdio version. */
/* The declaration must be identical to that in <stdio.h>. */
#ifdef __EMX__
int 
fprintf(FILE *file, __const__ char *fmt, ...)
#else
int 
fprintf(FILE *file, const char *fmt, ...)
#endif
{
int count;
va_list args;
	va_start(args,fmt);
	if ( gp_file_is_console(file) ) {
		char buf[1024];
		count = vsprintf(buf,fmt,args);
		(*pgsdll_callback)(GSDLL_STDOUT, buf, count);
	}
	else {
		count = vfprintf(file, fmt, args);
	}
	va_end(args);
	return count;
}
#endif /* __DLL__ */

