/* Copyright (C) 1992, 1995 Aladdin Enterprises.  All rights reserved.
  
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

/* echogs.c */
/* 'echo'-like utility */
#include <stdio.h>
/* Some brain-damaged environments (e.g. Sun) don't include */
/* prototypes for fputc/fputs in stdio.h! */
extern int fputc(), fputs();
/* Some systems have time_t in sys/types.h rather than time.h. */
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <time.h>		/* for ctime */
/* The VMS environment uses different values for success/failure exits: */
#ifdef VMS
#include <stdlib.h>
#  define exit_OK 1
#  define exit_FAILED 18
#else
#  define exit_OK 0
#  define exit_FAILED 1
#endif

/*
 * This program exists solely to get around omissions, problems, and
 * incompatibilities in various shells and utility environments.
 * Don't count on it staying the same from one release to another!
 */

/*
 * Usage:
	echogs [-e .extn] [-(w|a)[b][-] file] [-h] [-n]
	  (-D | -F | -x hexstring | -[q|Q} string | -[q|Q]string | -s |
	   -i | -r file | -R file | -X)*
	  [-] string*
 * Echoes string(s), or the binary equivalent of hexstring(s).
 * If -w, writes to file; if -a, appends to file; if neither,
 *   writes to stdout.  -wb and -ab open the file in binary mode.
 *   -w and -a search forward for the next argument that is not a switch.
 *   An appended - means the same as - alone, taking effect after the file
 *   argument.
 * -e specifies an extension to be added to the file name.
 * If -h, write the output in hex instead of literally.
 * If -n, does not append a newline to the output.
 * -D means insert the date and time.
 * -F means insert the file name passed as the argument of -w or -a.
 * -q means write the next string literally.
 * -Q means the same as -q followed by -s.
 * -s means write a space.
 * -i means read from stdin, treating each line as an argument.
 * -r means read from a named file in the same way.
 * -R means read from a named file with no interpretation
 *   (but convert to hex if -h is in effect).
 * -X means treat any following literals as hex rather than string data.
 * - alone means treat the rest of the line as literal data,
 *   even if the first string begins with a -.
 * Inserts spaces automatically between the trailing strings,
 * but nowhere else; in particular,
	echogs -q a b
 * writes 'ab', in contrast to
	echogs -q a -s b
 * which writes 'a b'.
 */

static int hputc(), hputs();

main(int argc, char *argv[])
{	FILE *out = stdout;
	FILE *in;
	char *extn = "";
	char fmode[4];
#define FNSIZE 100
	char *fnparam;
	char fname[FNSIZE];
	int newline = 1;
	int interact = 0;
	int (*eputc)() = fputc, (*eputs)() = fputs;
#define LINESIZE 1000
	char line[LINESIZE];
	char sw = 0, sp = 0, hexx = 0;
	char **argp = argv + 1;
	int nargs = argc - 1;
	if ( nargs > 0 && !strcmp(*argp, "-e") )
	{	if ( nargs < 2 ) return 1;
		extn = argp[1];
		argp += 2, nargs -= 2;
	}
	if ( nargs > 0 && (*argp)[0] == '-' &&
	      ((*argp)[1] == 'w' || (*argp)[1] == 'a')
	   )
	{	size_t len = strlen(*argp);
		int i;
		if ( len > 4 ) return 1;
		for ( i = 1; i < nargs; i++ )
		  if ( argp[i][0] != '-' ) break;
		if ( i == nargs ) return 1;
		fnparam = argp[i];
		strcpy(fmode, *argp + 1);
		strcpy(fname, fnparam);
		strcat(fname, extn);
		if ( fmode[len-2] == '-' )
		  {	fmode[len-2] = 0;
			argp[i] = "-";
			argp++, nargs--;
		  }
		else
		  {	for ( ; i > 1; i-- )
			  argp[i] = argp[i-1];
			argp += 2, nargs -= 2;
		  }
	}
	else
		strcpy(fname, "");
	if ( nargs > 0 && !strcmp(*argp, "-h") )
	{	eputc = hputc, eputs = hputs;
		argp++, nargs--;
	}
	if ( nargs > 0 && !strcmp(*argp, "-n") )
	{	newline = 0;
		argp++, nargs--;
	}
	if ( strlen(fname) != 0 )
	{	out = fopen(fname, fmode);
		if ( out == 0 ) return 1;
	}
	while ( 1 )
	{	char *arg;
		if ( interact )
		{	if ( fgets(line, LINESIZE, in) == NULL )
			{	interact = 0;
				if ( in != stdin ) fclose(in);
				continue;
			}
			/* Remove the terminating \n. */
			line[strlen(line) - 1] = 0;
			arg = line;
		}
		else
		{	if ( nargs == 0 ) break;
			arg = *argp;
			argp++, nargs--;
		}
		if ( sw == 0 && arg[0] == '-' )
		{	sp = 0;
			switch ( arg[1] )
			{
			case 'q':		/* literal string */
			case 'Q':		/* literal string, then -s */
				if ( arg[2] != 0 )
				  {	(*eputs)(arg + 2, out);
					if ( arg[1] == 'Q' )
					  (*eputc)(' ', out);
					break;
				  }
				/* falls through */
			case 'r':		/* read from a file */
			case 'R':		/* insert file literally */
			case 'x':		/* hex string */
				sw = arg[1];
				break;
			case 's':		/* write a space */
				(*eputc)(' ', out);
				break;
			case 'i':		/* read interactively */
				interact = 1;
				in = stdin;
				break;
			case 'D':		/* insert date/time */
			{	time_t t;
				char str[26];
				time(&t);
				strcpy(str, ctime(&t));
				str[24] = 0;	/* remove \n */
				(*eputs)(str, out);
			}	break;
			case 'F':		/* insert file name */
				(*eputs)(fnparam, out);
				break;
			case 'X':		/* treat literals as hex */
				hexx = 1;
				break;
			case 0:			/* just '-' */
				sw = '-';
				break;
			}
		}
		else
		  switch ( sw )
		{
		case 0:
		case '-':
			if ( hexx ) goto xx;
			if ( sp ) (*eputc)(' ', out);
			(*eputs)(arg, out);
			sp = 1;
			break;
		case 'q':
			sw = 0;
			(*eputs)(arg, out);
			break;
		case 'Q':
			sw = 0;
			(*eputs)(arg, out);
			(*eputc)(' ', out);
			break;
		case 'r':
			sw = 0;
			in = fopen(arg, "r");
			if ( in == NULL ) exit(exit_FAILED);
			interact = 1;
			break;
		case 'R':
			sw = 0;
			in = fopen(arg, "r");
			if ( in == NULL ) exit(exit_FAILED);
			{	int count;
				while ( (count = fread(line, 1, 1, in)) > 0 )
				  (*eputc)(line[0], out);
			}
			fclose(in);
			break;
		case 'x':
xx:		{	char *xp;
			unsigned int xchr = 1;
			for ( xp = arg; *xp; xp++ )
			{	char ch = *xp;
				if ( !isxdigit(ch) ) return 1;
				xchr <<= 4;
				xchr += (isdigit(ch) ? ch - '0' :
					 (isupper(ch) ? tolower(ch) : ch)
					  - 'a' + 10);
				if ( xchr >= 0x100 )
				{	(*eputc)(xchr & 0xff, out);
					xchr = 1;
				}
			}
		}	sw = 0;
			break;
		}
	}
	if ( newline ) (*eputc)('\n', out);
	if ( out != stdout ) fclose(out);
	return exit_OK;
}

static int
hputc(int ch, FILE *out)
{	static char *hex = "0123456789abcdef";
	putc(hex[ch >> 4], out);
	putc(hex[ch & 0xf], out);
	return 0;
}

static int
hputs(char *str, FILE *out)
{	while ( *str ) hputc(*str++ & 0xff, out);
	return 0;
}
