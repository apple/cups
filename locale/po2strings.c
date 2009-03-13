/*
 * "$Id: po2strings.c 6921 2007-09-06 13:38:37Z mike $"
 *
 * Convert GNU gettext .po files to Apple .strings file (UTF-16 BE text file).
 *
 * Usage:
 *
 *   po2strings filename.strings filename.po
 *
 * Compile with:
 *
 *   gcc -o po2strings po2strings.c `cups-config --libs`
 *
 * Contents:
 *
 *   main()         - Convert .po file to .strings.
 *   write_string() - Write a string to the .strings file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <cups/i18n.h>
#include <cups/string.h>


/*
 * The .strings file format is simple:
 *
 * // comment
 * "id" = "str";
 *
 * Both the id and str strings use standard C quoting for special characters
 * like newline and the double quote character.
 */

/*
 * Local functions...
 */

static void	write_string(FILE *strings, const char *s);


/*
 *   main() - Convert .po file to .strings.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  int			i;		/* Looping var */
  FILE			*strings;	/* .strings file */
  cups_array_t		*po;		/* .po file */
  char			iconv[1024];	/* iconv command */
  _cups_message_t	*msg;		/* Current message */
  const char		*srcfile,	/* Source file */
			*dstfile;	/* Destination file */
  int			use_msgid;	/* Use msgid strings for msgstr? */


  srcfile   = NULL;
  dstfile   = NULL;
  use_msgid = 0;


  for (i = 1; i < argc; i ++)
    if (!strcmp(argv[i], "-m"))
      use_msgid = 1;
    else if (argv[i][0] == '-')
    {
      puts("Usage: po2strings [-m] filename.po filename.strings");
      return (1);
    }
    else if (srcfile == NULL)
      srcfile = argv[i];
    else if (dstfile == NULL)
      dstfile = argv[i];
    else
    {
      puts("Usage: po2strings [-m] filename.po filename.strings");
      return (1);
    }

  if (!srcfile || !dstfile)
  {
    puts("Usage: po2strings [-m] filename.po filename.strings");
    return (1);
  }

 /*
  * Use the CUPS .po loader to get the message strings...
  */

  if ((po = _cupsMessageLoad(srcfile, 1)) == NULL)
  {
    perror(srcfile);
    return (1);
  }

 /*
  * Cheat by using iconv to write the .strings file with a UTF-16 encoding.
  * The .po file uses UTF-8...
  */

  snprintf(iconv, sizeof(iconv), "iconv -f utf-8 -t utf-16 >'%s'", dstfile);
  if ((strings = popen(iconv, "w")) == NULL)
  {
    perror(argv[2]);
    _cupsMessageFree(po);
    return (1);
  }

  for (msg = (_cups_message_t *)cupsArrayFirst(po);
       msg;
       msg = (_cups_message_t *)cupsArrayNext(po))
  {
    write_string(strings, msg->id);
    fputs(" = ", strings);
    write_string(strings, use_msgid ? msg->id : msg->str);
    fputs(";\n", strings);
  }

  printf("%s: %d messages.\n", argv[2], cupsArrayCount(po));

  pclose(strings);
  _cupsMessageFree(po);

  return (0);
}


/*
 * 'write_string()' - Write a string to the .strings file.
 */

static void
write_string(FILE       *strings,	/* I - .strings file */
             const char *s)		/* I - String to write */
{
  putc('\"', strings);

  while (*s)
  {
    switch (*s)
    {
      case '\n' :
          fputs("\\n", strings);
	  break;
      case '\t' :
          fputs("\\t", strings);
	  break;
      case '\\' :
          fputs("\\\\", strings);
	  break;
      case '\"' :
          fputs("\\\"", strings);
	  break;
      default :
          putc(*s, strings);
	  break;
    }

    s ++;
  }

  putc('\"', strings);
}


/*
 * End of "$Id: po2strings.c 6921 2007-09-06 13:38:37Z mike $".
 */
