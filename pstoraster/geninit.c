/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* geninit.c */
/* Utility for merging all the Ghostscript initialization files */
/* (gs_*.ps) into a single file, optionally converting them to C data. */
#include "stdio_.h"
#include "string_.h"

/* Usage:
 *	geninit <init-file.ps> <gconfig.h> <merged-init-file.ps>
 *	geninit <init-file.ps> <gconfig.h> -c <merged-init-file.c>
 */

/* Forward references */
private void merge_to_c();
private void merge_to_ps();

#define line_size 128

int
main(int argc, char *argv[])
{	const char *fin;
	FILE *in;
	const char *fconfig;
	FILE *config;
	const char *fout;
	FILE *out;
	bool to_c = false;

	if ( argc == 4 )
	  fin = argv[1], fconfig = argv[2], fout = argv[3];
	else if ( argc == 5 && !strcmp(argv[3], "-c") )
	  fin = argv[1], fconfig = argv[2], fout = argv[4], to_c = true;
	else
	  { fprintf(stderr, "\
Usage: geninit gs_init.ps gconfig.h gs_xinit.ps\n\
 or    geninit gs_init.ps gconfig.h -c gs_init.c\n");
	    exit(1);
	  }
	in = fopen(fin, "r");
	if ( in == 0 )
	  { fprintf(stderr, "Cannot open %s for reading.\n", fin);
	    exit(1);
	  }
	config = fopen(fconfig, "r");
	if ( config == 0 )
	  { fprintf(stderr, "Cannot open %s for reading.\n", fconfig);
	    fclose(in);
	    exit(1);
	  }
	out = fopen(fout, "w");
	if ( out == 0 )
	  { fprintf(stderr, "Cannot open %s for writing.\n", fout);
	    fclose(config);
	    fclose(in);
	    exit(1);
	  }
	if ( to_c )
	  merge_to_c(fin, in, config, out);
	else
	  merge_to_ps(fin, in, config, out);
	fclose(out);
	return 0;
}

/* Read a line from the input. */
private bool
rl(FILE *in, char *str, int len)
{	if ( fgets(str, len, in) == NULL )
	  return false;
	str[strlen(str) - 1] = 0;	/* remove newline */
	return true;
}

/* Write a line on the output. */
private void
wl(FILE *out, const char *str, bool to_c)
{	if ( to_c )
	  { int n = 0;
	    const char *p = str;
	    for ( ; *p; ++p )
	      { char c = *p;
		const char *format = "%d,";
		if ( c >= 32 && c < 127 )
		  format = (c == '\'' || c == '\\' ? "'\\%c'," : "'%c',");
		fprintf(out, format, c);
		if ( ++n == 15 )
		  { fputs("\n", out);
		    n = 0;
		  }
	      }
	    fputs("10,\n", out);
	  }
	else
	  { fprintf(out, "%s\n", str);
	  }
}

/* Strip whitespace and comments from a string if possible. */
/* Return a pointer to any string that remains, or NULL if none. */
/* Note that this may store into the string. */
private char *
doit(char *line)
{	char *str = line;
	char *p1;

	while ( *str == ' ' || *str == '\t' )	/* strip leading whitespace */
	  ++str;
	if ( *str == 0 )		/* all whitespace */
	  return NULL;
	if ( !strncmp(str, "%END", 4) )	/* keep these for .skipeof */
	  return str;
	if ( str[0] == '%' )		/* comment line */
	  return NULL;
	if ( (p1 = strchr(str, '%')) == NULL )	/* no internal comment */
	  return str;
	if ( strchr(p1, ')') != NULL )	/* might be a % inside a string */
	  return str;
	while ( p1[-1] == ' ' || p1[-1] == '\t' )
	  --p1;
	*p1 = 0;			/* remove comment */
	return str;
}

/* Merge a file from input to output. */
private void
mergefile(const char *inname, FILE *in, FILE *config, FILE *out, bool to_c)
{	char line[line_size + 1];

	while ( rl(in, line, line_size) )
	  { char psname[line_size + 1];
	    int nlines;

	    if ( !strncmp(line, "%% Replace ", 11) &&
		 sscanf(line + 11, "%d %s", &nlines, psname) == 2
	       )
	      { while ( nlines-- > 0 )
		  rl(in, line, line_size);
		if ( psname[0] == '(' )
		  { FILE *ps;
		    psname[strlen(psname) - 1] = 0;
		    ps = fopen(psname + 1, "r");
		    if ( ps == 0 )
		      { fprintf(stderr, "Cannot open %s for reading.\n", psname + 1);
			exit(1);
		      }
		    mergefile(psname + 1, ps, config, out, to_c);
		  }
		else if ( !strcmp(psname, "INITFILES") )
		  { /*
		     * We don't want to bind config.h into geninit, so
		     * we parse it ourselves at execution time instead.
		     */
		    rewind(config);
		    while ( rl(config, psname, line_size) )
		      if ( !strncmp(psname, "psfile_(\"", 9) )
			{ FILE *ps;
			  psname[strlen(psname) - 2] = 0;
			  ps = fopen(psname + 9, "r");
			  if ( ps == 0 )
			    { fprintf(stderr, "Cannot open %s for reading.\n", psname + 9);
			      exit(1);
			    }
			  mergefile(psname + 9, ps, config, out, to_c);
			}
		  }
		else
		  { fprintf(stderr, "Unknown %%%% Replace %d %s\n",
			    nlines, psname);
		    exit(1);
		  }
	      }
	    else if ( !strcmp(line, "currentfile closefile") )
	      { /* The rest of the file is debugging code, stop here. */
		break;
	      }
	    else
	      { char *str = doit(line);
		if ( str != 0 )
		  wl(out, str, to_c);
	      }
	  }
	fprintf(stderr, "%s: %ld bytes, output pos = %ld\n",
		inname, ftell(in), ftell(out));
	fclose(in);
}

/* Merge and produce a C file. */
private void
merge_to_c(const char *inname, FILE *in, FILE *config, FILE *out)
{	char line[line_size + 1];

	fputs("/*\n", out);
	while ( (rl(in, line, line_size), line[0]) )
	  fprintf(out, "%s\n", line);
	fputs("*/\n", out);
	fputs("\n", out);
	fputs("/* Pre-compiled interpreter initialization string. */\n", out);
	fputs("#include \"stdpre.h\"\n", out);
	fputs("\n", out);
	fputs("const byte gs_init_string[] = {\n", out);
	mergefile(inname, in, config, out, true);
	fputs("10};\n", out);
	fputs("const uint gs_init_string_sizeof = sizeof(gs_init_string);\n", out);
}

/* Merge and produce a PostScript file. */
private void
merge_to_ps(const char *inname, FILE *in, FILE *config, FILE *out)
{	char line[line_size + 1];

	while ( (rl(in, line, line_size), line[0]) )
	  fprintf(out, "%s\n", line);
	mergefile(inname, in, config, out, false);
}
