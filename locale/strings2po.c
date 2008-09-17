/*
 * "$Id: strings2po.c 7720 2008-07-11 22:46:21Z mike $"
 *
 * Convert Apple .strings file (UTF-16 BE text file) to GNU gettext .po files.
 *
 * Usage:
 *
 *   strings2po filename.strings filename.po
 *
 * Compile with:
 *
 *   gcc -o strings2po strings2po.c
 *
 * Contents:
 *
 *   main()         - Convert .strings file to .po.
 *   read_strings() - Read a line from a .strings file.
 *   write_po()     - Write a line to the .po file.
 */

#include <stdio.h>
#include <stdlib.h>


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

static int	read_strings(FILE *strings, char *buffer, size_t bufsize,
		             char **id, char **str);
static void	write_po(FILE *po, const char *what, const char *s);


/*
 *   main() - Convert .strings file to .po.
 */

int					/* O - Exit code */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line arguments */
{
  FILE	*strings,			/* .strings file */
	*po;				/* .po file */
  char	iconv[1024],			/* iconv command */
	buffer[8192],			/* Line buffer */
	*id,				/* ID string */
	*str;				/* Translation string */
  int	count;				/* Number of messages converted */


  if (argc != 3)
  {
    puts("Usage: strings2po filename.strings filename.po");
    return (1);
  }

 /*
  * Cheat by using iconv to convert the .strings file from UTF-16 to UTF-8
  * which is what we need for the .po file (and it makes things a lot
  * simpler...)
  */

  snprintf(iconv, sizeof(iconv), "iconv -f utf-16 -t utf-8 '%s'", argv[1]);
  if ((strings = popen(iconv, "r")) == NULL)
  {
    perror(argv[1]);
    return (1);
  }

  if ((po = fopen(argv[2], "w")) == NULL)
  {
    perror(argv[2]);
    pclose(strings);
    return (1);
  }

  count = 0;

  while (read_strings(strings, buffer, sizeof(buffer), &id, &str))
  {
    count ++;
    write_po(po, "msgid", id);
    write_po(po, "msgstr", str);
  }

  pclose(strings);
  fclose(po);

  printf("%s: %d messages.\n", argv[2], count);

  return (0);
}


/*
 * 'read_strings()' - Read a line from a .strings file.
 */

static int				/* O - 1 on success, 0 on failure */
read_strings(FILE   *strings,		/* I - .strings file */
             char   *buffer,		/* I - Line buffer */
	     size_t bufsize,		/* I - Size of line buffer */
             char   **id,		/* O - Pointer to ID string */
	     char   **str)		/* O - Pointer to translation string */
{
  char	*bufptr;			/* Pointer into buffer */


  while (fgets(buffer, bufsize, strings))
  {
    if (buffer[0] != '\"')
      continue;

    *id = buffer + 1;

    for (bufptr = buffer + 1; *bufptr && *bufptr != '\"'; bufptr ++)
      if (*bufptr == '\\')
        bufptr ++;

    if (*bufptr != '\"')
      continue;

    *bufptr++ = '\0';

    while (*bufptr && *bufptr != '\"')
      bufptr ++;

    if (!*bufptr)
      continue;

    bufptr ++;
    *str = bufptr;

    for (; *bufptr && *bufptr != '\"'; bufptr ++)
      if (*bufptr == '\\')
        bufptr ++;

    if (*bufptr != '\"')
      continue;

    *bufptr = '\0';

    return (1);
  }

  return (0);
}


/*
 * 'write_po()' - Write a line to the .po file.
 */

static void
write_po(FILE       *po,		/* I - .po file */
         const char *what,		/* I - Type of string */
	 const char *s)			/* I - String to write */
{
  fprintf(po, "%s \"%s\"\n", what, s);
}


/*
 * End of "$Id: strings2po.c 7720 2008-07-11 22:46:21Z mike $".
 */
