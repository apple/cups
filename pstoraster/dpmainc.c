/* Copyright (C) 1996, Russell Lang.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/


/* dpmainc.c */
/* Ghostscript DLL loader for OS/2 */
/* For WINDOWCOMPAT (console mode) application */

/* Russell Lang  1996-06-05 */

#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include "gscdefs.h"
#define GS_REVISION gs_revision
#include "gsdll.h"

#define MAXSTR 256
const char *szDllName = "GSDLL2.DLL";
char start_string[] = "systemdict /start get exec\n";
int debug = FALSE;

/* main structure with info about the GS DLL */
typedef struct tagGSDLL {
	BOOL		valid;		/* true if loaded */
	HMODULE		hmodule;	/* handle to module */
	/* pointers to DLL functions */
	PFN_gsdll_revision	revision;
	PFN_gsdll_init		init;
	PFN_gsdll_exit		exit;
	PFN_gsdll_execute_begin	execute_begin;
	PFN_gsdll_execute_cont	execute_cont;
	PFN_gsdll_execute_end	execute_end;
	PFN_gsdll_get_bitmap	get_bitmap;
	PFN_gsdll_lock_device	lock_device;
	/* pointer to os2dll device */
	char	*device;
} GSDLL;
GSDLL gsdll;

void
gs_addmess(char *str)
{
    fputs(str, stdout);
}

/* free GS DLL */
/* This should only be called when gsdll_execute has returned */
/* TRUE means no error */
BOOL
gs_free_dll(void)
{
char buf[MAXSTR];
APIRET rc;
	if (gsdll.hmodule == (HMODULE)NULL)
	    return TRUE;
	rc = DosFreeModule(gsdll.hmodule);
	if (rc) {
	    sprintf(buf,"DosFreeModule returns %d\n", rc);
	    gs_addmess(buf);
	    sprintf(buf,"Unloaded GSDLL\n\n");
	    gs_addmess(buf);
	}
	return !rc;
}

void
gs_load_dll_cleanup(void)
{
char buf[MAXSTR];
    gs_free_dll();
    fprintf(stdout, "Can't load Ghostscript DLL %s", szDllName);
}

/* load GS DLL if not already loaded */
/* return TRUE if OK */
BOOL
gs_load_dll(void)
{
char buf[MAXSTR+40];
APIRET rc;
char *p;
int i;
long revision;
const char *dllname;
PTIB pptib;
PPIB pppib;
char szExePath[MAXSTR];
char fullname[1024];
const char *shortname;

    if ( (rc = DosGetInfoBlocks(&pptib, &pppib)) != 0 ) {
	fprintf(stdout,"Couldn't get pid, rc = \n", rc);
	return FALSE;
    }

    /* get path to EXE */
    if ( (rc = DosQueryModuleName(pppib->pib_hmte, sizeof(szExePath), szExePath)) != 0 ) {
	fprintf(stdout,"Couldn't get module name, rc = %d\n", rc);
	return FALSE;
    }
    if ((p = strrchr(szExePath,'\\')) != (char *)NULL) {
	p++;
	*p = '\0';
    }

	dllname = szDllName;
	if (debug) {
	    sprintf(buf, "Trying to load %s\n", dllname);
	    gs_addmess(buf);
	}
	memset(buf, 0, sizeof(buf));
	rc = DosLoadModule(buf, sizeof(buf), dllname, &gsdll.hmodule);
	if (rc) {
	    /* failed */
	    /* try again, with path of EXE */
	    if ((shortname = strrchr((char *)szDllName, '\\')) == (const char *)NULL)
		shortname = szDllName;
	    strcpy(fullname, szExePath);
	    if ((p = strrchr(fullname,'\\')) != (char *)NULL)
		p++;
	    else
		p = fullname;
	    *p = '\0';
	    strcat(fullname, shortname);
	    dllname = fullname;
	    if (debug) {
	        sprintf(buf, "Trying to load %s\n", dllname);
	        gs_addmess(buf);
	    }
	    rc = DosLoadModule(buf, sizeof(buf), dllname, &gsdll.hmodule);
	    if (rc) {
		/* failed again */
		/* try once more, this time on system search path */
		dllname = shortname;
		if (debug) {
		    sprintf(buf, "Trying to load %s\n", dllname);
		    gs_addmess(buf);
		}
	        rc = DosLoadModule(buf, sizeof(buf), dllname, &gsdll.hmodule);
	    }
	}


	if (rc == 0) {
	    if (debug)
	        gs_addmess("Loaded Ghostscript DLL\n");
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_REVISION", (PFN *)(&gsdll.revision)))!=0) {
	        sprintf(buf, "Can't find GSDLL_REVISION, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    /* check DLL version */
	    gsdll.revision(NULL, NULL, &revision, NULL);
	    if (revision != GS_REVISION) {
		sprintf(buf, "Wrong version of DLL found.\n  Found version %ld\n  Need version  %ld\n", revision, (long)GS_REVISION);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_INIT", (PFN *)(&gsdll.init)))!=0) {
	        sprintf(buf, "Can't find GSDLL_INIT, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_EXECUTE_BEGIN", (PFN *)(&gsdll.execute_begin)))!=0) {
	        sprintf(buf, "Can't find GSDLL_EXECUTE_BEGIN, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_EXECUTE_CONT", (PFN *)(&gsdll.execute_cont)))!=0) {
	        sprintf(buf, "Can't find GSDLL_EXECUTE_CONT, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_EXECUTE_END", (PFN *)(&gsdll.execute_end)))!=0) {
	        sprintf(buf, "Can't find GSDLL_EXECUTE_END, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_EXIT", (PFN *)(&gsdll.exit)))!=0) {
	        sprintf(buf, "Can't find GSDLL_EXIT, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_GET_BITMAP", (PFN *)(&gsdll.get_bitmap)))!=0) {
	        sprintf(buf, "Can't find GSDLL_GET_BITMAP, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	    if ((rc = DosQueryProcAddr(gsdll.hmodule, 0, "GSDLL_LOCK_DEVICE", (PFN *)(&gsdll.lock_device)))!=0) {
	        sprintf(buf, "Can't find GSDLL_LOCK_DEVICE, rc = %d\n", rc);
		gs_addmess(buf);
		gs_load_dll_cleanup();
		return FALSE;
	    }
	}
	else {
	    sprintf(buf, "Can't load Ghostscript DLL %s \nDosLoadModule rc = %d\n", szDllName, rc);
	    gs_addmess(buf);
	    gs_load_dll_cleanup();
	    return FALSE;
	}
	return TRUE;
}


int
read_stdin(char FAR *str, int len)
{
int ch;
int count = 0;
    while (count < len) {
	ch = fgetc(stdin);
	if (ch == EOF)
	    return count;
	*str++ = ch;
	count++;
	if (ch == '\n')
	    return count;
    }
    return count;
}

int 
gsdll_callback(int message, char *str, unsigned long count)
{
char *p;
    switch (message) {
	case GSDLL_STDIN:
	    return read_stdin(str, count);
	case GSDLL_STDOUT:
	    if (str != (char *)NULL)
		fwrite(str, 1, count, stdout);
	    fflush(stdout);
	    return count;
	case GSDLL_DEVICE:
	    if (count)
		fprintf(stdout, "os2dll device not supported in this version of Ghostscript\n");
	    fprintf(stdout,"Callback: DEVICE %p %s\n", str,
		count ? "open" : "close");
	    break;
	case GSDLL_SYNC:
	    fprintf(stdout,"Callback: SYNC %p\n", str);
	    break;
	case GSDLL_PAGE:
	    fprintf(stdout,"Callback: PAGE %p\n", str);
	    break;
	case GSDLL_SIZE:
	    fprintf(stdout,"Callback: SIZE %p width=%d height=%d\n", str,
		(int)(count & 0xffff), (int)((count>>16) & 0xffff) );
	    break;
	case GSDLL_POLL:
	    return 0; /* no error */
	default:
	    fprintf(stdout,"Callback: Unknown message=%d\n",message);
	    break;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
int code;
    if (!gs_load_dll()) {
	fprintf(stderr, "Can't load %s\n", szDllName);
	return -1;
    }
    code = gsdll.init(gsdll_callback, (HWND)NULL, argc, argv);
    if (!code)
        code = gsdll.execute_begin();
    if (!code) {
	code = gsdll.execute_cont(start_string, strlen(start_string));
	if (!code) {
	    gsdll.execute_end();
	    gsdll.exit();
	}
	else
	    code = gsdll.exit();
    }
    gs_free_dll();
    if (code == GSDLL_INIT_QUIT)
	return 0;
    return code;
}
