/*
 * "$Id$"
 *
 *   File test program for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   main()             - Main entry.
 *   read_write_tests() - Perform read/write tests.
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "string.h"
#include "file.h"
#include "debug.h"
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */
#include <fcntl.h>


/*
 * Local functions...
 */

static int	read_write_tests(int compression);


/*
 * 'main()' - Main entry.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int		status;			/* Exit status */
  char		filename[1024];		/* Filename buffer */
  int		fds[2];			/* Open file descriptors */
  cups_file_t	*fdfile;		/* File opened with cupsFileOpenFd() */


  if (argc == 1)
  {
   /*
    * Do uncompressed file tests...
    */

    status = read_write_tests(0);

#ifdef HAVE_LIBZ
   /*
    * Do compressed file tests...
    */

    putchar('\n');

    status += read_write_tests(1);
#endif /* HAVE_LIBZ */

   /*
    * Test fdopen and close without reading...
    */

    pipe(fds);
    close(fds[1]);

    fputs("cupsFileOpenFd(fd, \"r\"): ", stdout);
    fflush(stdout);

    if ((fdfile = cupsFileOpenFd(fds[0], "r")) == NULL)
    {
      puts("FAIL");
      status ++;
    }
    else
    {
     /*
      * Able to open file, now close without reading.  If we don't return
      * before the alarm fires, that is a failure and we will crash on the
      * alarm signal...
      */

      puts("PASS");
      fputs("cupsFileClose(no read): ", stdout);
      fflush(stdout);

      alarm(5);
      cupsFileClose(fdfile);
      alarm(0);

      puts("PASS");
    }

   /*
    * Test path functions...
    */

    fputs("cupsFileFind: ", stdout);
#ifdef WIN32
    if (cupsFileFind("notepad.exe", "C:/WINDOWS", 1, filename, sizeof(filename)) &&
	cupsFileFind("notepad.exe", "C:/WINDOWS;C:/WINDOWS/SYSTEM32", 1, filename, sizeof(filename)))
#else
    if (cupsFileFind("cat", "/bin", 1, filename, sizeof(filename)) &&
	cupsFileFind("cat", "/bin:/usr/bin", 1, filename, sizeof(filename)))
#endif /* WIN32 */
      printf("PASS (%s)\n", filename);
    else
    {
      puts("FAIL");
      status ++;
    }

   /*
    * Summarize the results and return...
    */

    if (!status)
      puts("\nALL TESTS PASSED!");
    else
      printf("\n%d TEST(S) FAILED!\n", status);
  }
  else
  {
   /*
    * Cat the filename on the command-line...
    */

    cups_file_t	*fp;			/* File pointer */
    char	line[1024];		/* Line from file */


    if ((fp = cupsFileOpen(argv[1], "r")) == NULL)
    {
      perror(argv[1]);
      status = 1;
    }
    else
    {
      status = 0;

      while (cupsFileGets(fp, line, sizeof(line)))
        puts(line);

      if (!cupsFileEOF(fp))
        perror(argv[1]);

      cupsFileClose(fp);
    }
  }

  return (status);
}


/*
 * 'read_write_tests()' - Perform read/write tests.
 */

static int				/* O - Status */
read_write_tests(int compression)	/* I - Use compression? */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* First file */
  int		status;			/* Exit status */
  char		line[1024],		/* Line from file */
		*value;			/* Directive value from line */
  int		linenum;		/* Line number */
  unsigned char	readbuf[8192],		/* Read buffer */
		writebuf[8192];		/* Write buffer */
  int		byte;			/* Byte from file */
  static const char *partial_line = "partial line";
					/* Partial line */


 /*
  * No errors so far...
  */

  status = 0;

 /*
  * Initialize the write buffer with random data...
  */

#ifdef WIN32
  srand((unsigned)time(NULL));
#else
  srand(time(NULL));
#endif /* WIN32 */

  for (i = 0; i < (int)sizeof(writebuf); i ++)
    writebuf[i] = rand();

 /*
  * cupsFileOpen(write)
  */

  printf("cupsFileOpen(write%s): ", compression ? " compressed" : "");

  fp = cupsFileOpen(compression ? "testfile.dat.gz" : "testfile.dat",
                    compression ? "w9" : "w");
  if (fp)
  {
    puts("PASS");

   /*
    * cupsFileCompression()
    */

    fputs("cupsFileCompression(): ", stdout);

    if (cupsFileCompression(fp) == compression)
      puts("PASS");
    else
    {
      printf("FAIL (Got %d, expected %d)\n", cupsFileCompression(fp),
             compression);
      status ++;
    }

   /*
    * cupsFilePuts()
    */

    fputs("cupsFilePuts(): ", stdout);

    if (cupsFilePuts(fp, "# Hello, World\n") > 0)
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFilePrintf()
    */

    fputs("cupsFilePrintf(): ", stdout);

    for (i = 0; i < 1000; i ++)
      if (cupsFilePrintf(fp, "TestLine %d\n", i) < 0)
        break;

    if (i >= 1000)
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFilePutChar()
    */

    fputs("cupsFilePutChar(): ", stdout);

    for (i = 0; i < 256; i ++)
      if (cupsFilePutChar(fp, i) < 0)
        break;

    if (i >= 256)
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileWrite()
    */

    fputs("cupsFileWrite(): ", stdout);

    for (i = 0; i < 10000; i ++)
      if (cupsFileWrite(fp, (char *)writebuf, sizeof(writebuf)) < 0)
        break;

    if (i >= 10000)
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFilePuts() with partial line...
    */

    fputs("cupsFilePuts(\"partial line\"): ", stdout);

    if (cupsFilePuts(fp, partial_line) > 0)
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileClose()
    */

    fputs("cupsFileClose(): ", stdout);

    if (!cupsFileClose(fp))
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }
  }
  else
  {
    printf("FAIL (%s)\n", strerror(errno));
    status ++;
  }

 /*
  * cupsFileOpen(read)
  */

  fputs("cupsFileOpen(read): ", stdout);

  fp = cupsFileOpen(compression ? "testfile.dat.gz" : "testfile.dat", "r");
  if (fp)
  {
    puts("PASS");

   /*
    * cupsFileGets()
    */

    fputs("cupsFileGets(): ", stdout);

    if (cupsFileGets(fp, line, sizeof(line)))
    {
      if (line[0] == '#')
        puts("PASS");
      else
      {
        printf("FAIL (Got line \"%s\", expected comment line)\n", line);
	status ++;
      }
    }
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileCompression()
    */

    fputs("cupsFileCompression(): ", stdout);

    if (cupsFileCompression(fp) == compression)
      puts("PASS");
    else
    {
      printf("FAIL (Got %d, expected %d)\n", cupsFileCompression(fp),
             compression);
      status ++;
    }

   /*
    * cupsFileGetConf()
    */

    linenum = 1;

    fputs("cupsFileGetConf(): ", stdout);

    for (i = 0; i < 1000; i ++)
      if (!cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
        break;
      else if (strcasecmp(line, "TestLine") || !value || atoi(value) != i ||
               linenum != (i + 2))
        break;

    if (i >= 1000)
      puts("PASS");
    else if (line[0])
    {
      printf("FAIL (Line %d, directive \"%s\", value \"%s\")\n", linenum,
             line, value ? value : "(null)");
      status ++;
    }
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileGetChar()
    */

    fputs("cupsFileGetChar(): ", stdout);

#ifdef DEBUG
    puts("\ni     byte\n----- -----");

    for (i = 0; i < 256; i ++)
    {
      byte = cupsFileGetChar(fp);

      printf("%-5d %-5d\n", i, byte);

      if (byte != i)
        break;
    }
#else
    for (i = 0; i < 256; i ++)
      if ((byte = cupsFileGetChar(fp)) != i)
        break;
#endif /* DEBUG */

    if (i >= 256)
      puts("PASS");
    else if (byte >= 0)
    {
      printf("FAIL (Got %d, expected %d)\n", byte, i);
      status ++;
    }
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileRead()
    */

    fputs("cupsFileRead(): ", stdout);

    for (i = 0; i < 10000; i ++)
      if ((byte = cupsFileRead(fp, (char *)readbuf, sizeof(readbuf))) < 0)
        break;
      else if (memcmp(readbuf, writebuf, sizeof(readbuf)))
        break;

    if (i >= 10000)
      puts("PASS");
    else if (byte > 0)
    {
      printf("FAIL (Pass %d, ", i);

      for (i = 0; i < (int)sizeof(readbuf); i ++)
        if (readbuf[i] != writebuf[i])
	  break;

      printf("match failed at offset %d - got %02X, expected %02X)\n",
             i, readbuf[i], writebuf[i]);
    }
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }

   /*
    * cupsFileGetChar() with partial line...
    */

    fputs("cupsFileGetChar(partial line): ", stdout);

    for (i = 0; i < strlen(partial_line); i ++)
      if ((byte = cupsFileGetChar(fp)) < 0)
        break;
      else if (byte != partial_line[i])
        break;

    if (!partial_line[i])
      puts("PASS");
    else
    {
      printf("FAIL (got '%c', expected '%c')\n", byte, partial_line[i]);
      status ++;
    }

   /*
    * cupsFileClose()
    */

    fputs("cupsFileClose(): ", stdout);

    if (!cupsFileClose(fp))
      puts("PASS");
    else
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
    }
  }
  else
  {
    printf("FAIL (%s)\n", strerror(errno));
    status ++;
  }

 /*
  * Return the test status...
  */

  return (status);
}


/*
 * End of "$Id$".
 */
