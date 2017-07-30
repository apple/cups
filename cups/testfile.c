/*
 * "$Id: testfile.c 12577 2015-03-30 19:07:17Z msweet $"
 *
 * File test program for CUPS.
 *
 * Copyright 2007-2015 by Apple Inc.
 * Copyright 1997-2007 by Easy Software Products.
 *
 * These coded instructions, statements, and computer programs are the
 * property of Apple Inc. and are protected by Federal copyright
 * law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 * which should have been included with this file.  If this file is
 * file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * This file is subject to the Apple OS-Developed Software exception.
 */

/*
 * Include necessary headers...
 */

#include "string-private.h"
#include "debug-private.h"
#include "file.h"
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_LIBZ
#  include <zlib.h>
#endif /* HAVE_LIBZ */
#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 */
#include <fcntl.h>


/*
 * Local functions...
 */

static int	count_lines(cups_file_t *fp);
static int	random_tests(void);
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
  cups_file_t	*fp;			/* File pointer */
#ifndef WIN32
  int		fds[2];			/* Open file descriptors */
  cups_file_t	*fdfile;		/* File opened with cupsFileOpenFd() */
#endif /* !WIN32 */
  int		count;			/* Number of lines in file */


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
    * Do uncompressed random I/O tests...
    */

    status += random_tests();

#ifndef WIN32
   /*
    * Test fdopen and close without reading...
    */

    pipe(fds);
    close(fds[1]);

    fputs("\ncupsFileOpenFd(fd, \"r\"): ", stdout);
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
#endif /* !WIN32 */

   /*
    * Count lines in psglyphs, rewind, then count again.
    */

    fputs("\ncupsFileOpen(\"../data/media.defs\", \"r\"): ", stdout);

    if ((fp = cupsFileOpen("../data/media.defs", "r")) == NULL)
    {
      puts("FAIL");
      status ++;
    }
    else
    {
      puts("PASS");
      fputs("cupsFileGets: ", stdout);

      if ((count = count_lines(fp)) != 208)
      {
        printf("FAIL (got %d lines, expected 208)\n", count);
	status ++;
      }
      else
      {
        puts("PASS");
	fputs("cupsFileRewind: ", stdout);

	if (cupsFileRewind(fp) != 0)
	{
	  puts("FAIL");
	  status ++;
	}
	else
	{
	  puts("PASS");
	  fputs("cupsFileGets: ", stdout);

	  if ((count = count_lines(fp)) != 208)
	  {
	    printf("FAIL (got %d lines, expected 208)\n", count);
	    status ++;
	  }
	  else
	    puts("PASS");
        }
      }

      cupsFileClose(fp);
    }

   /*
    * Test path functions...
    */

    fputs("\ncupsFileFind: ", stdout);
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

    char	line[8192];		/* Line from file */

    if ((fp = cupsFileOpen(argv[1], "r")) == NULL)
    {
      perror(argv[1]);
      status = 1;
    }
    else if (argc == 2)
    {
      status = 0;

      while (cupsFileGets(fp, line, sizeof(line)))
        puts(line);

      if (!cupsFileEOF(fp))
        perror(argv[1]);

      cupsFileClose(fp);
    }
    else
    {
      status = 0;
      ssize_t bytes;

      while ((bytes = cupsFileRead(fp, line, sizeof(line))) > 0)
        printf("%s: %d bytes\n", argv[1], (int)bytes);

      if (cupsFileEOF(fp))
        printf("%s: EOF\n", argv[1]);
      else
        perror(argv[1]);

      cupsFileClose(fp);
    }
  }

  return (status);
}


/*
 * 'count_lines()' - Count the number of lines in a file.
 */

static int				/* O - Number of lines */
count_lines(cups_file_t *fp)		/* I - File to read from */
{
  int	count;				/* Number of lines */
  char	line[1024];			/* Line buffer */


  for (count = 0; cupsFileGets(fp, line, sizeof(line)); count ++);

  return (count);
}


/*
 * 'random_tests()' - Do random access tests.
 */

static int				/* O - Status */
random_tests(void)
{
  int		status,			/* Status of tests */
		pass,			/* Current pass */
		count,			/* Number of records read */
		record,			/* Current record */
		num_records;		/* Number of records */
  off_t		pos;			/* Position in file */
  ssize_t	expected;		/* Expected position in file */
  cups_file_t	*fp;			/* File */
  char		buffer[512];		/* Data buffer */


 /*
  * Run 4 passes, each time appending to a data file and then reopening the
  * file for reading to validate random records in the file.
  */

  for (status = 0, pass = 0; pass < 4; pass ++)
  {
   /*
    * cupsFileOpen(append)
    */

    printf("\ncupsFileOpen(append %d): ", pass);

    if ((fp = cupsFileOpen("testfile.dat", "a")) == NULL)
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
      break;
    }
    else
      puts("PASS");

   /*
    * cupsFileTell()
    */

    expected = 256 * (ssize_t)sizeof(buffer) * pass;

    fputs("cupsFileTell(): ", stdout);
    if ((pos = cupsFileTell(fp)) != (off_t)expected)
    {
      printf("FAIL (" CUPS_LLFMT " instead of " CUPS_LLFMT ")\n",
	     CUPS_LLCAST pos, CUPS_LLCAST expected);
      status ++;
      break;
    }
    else
      puts("PASS");

   /*
    * cupsFileWrite()
    */

    fputs("cupsFileWrite(256 512-byte records): ", stdout);
    for (record = 0; record < 256; record ++)
    {
      memset(buffer, record, sizeof(buffer));
      if (cupsFileWrite(fp, buffer, sizeof(buffer)) < (ssize_t)sizeof(buffer))
        break;
    }

    if (record < 256)
    {
      printf("FAIL (%d: %s)\n", record, strerror(errno));
      status ++;
      break;
    }
    else
      puts("PASS");

   /*
    * cupsFileTell()
    */

    expected += 256 * (ssize_t)sizeof(buffer);

    fputs("cupsFileTell(): ", stdout);
    if ((pos = cupsFileTell(fp)) != (off_t)expected)
    {
      printf("FAIL (" CUPS_LLFMT " instead of " CUPS_LLFMT ")\n",
             CUPS_LLCAST pos, CUPS_LLCAST expected);
      status ++;
      break;
    }
    else
      puts("PASS");

    cupsFileClose(fp);

   /*
    * cupsFileOpen(read)
    */

    printf("\ncupsFileOpen(read %d): ", pass);

    if ((fp = cupsFileOpen("testfile.dat", "r")) == NULL)
    {
      printf("FAIL (%s)\n", strerror(errno));
      status ++;
      break;
    }
    else
      puts("PASS");

   /*
    * cupsFileSeek, cupsFileRead
    */

    fputs("cupsFileSeek(), cupsFileRead(): ", stdout);

    for (num_records = (pass + 1) * 256, count = (pass + 1) * 256, record = ((int)CUPS_RAND() & 65535) % num_records;
         count > 0;
	 count --, record = (record + ((int)CUPS_RAND() & 31) - 16 + num_records) % num_records)
    {
     /*
      * The last record is always the first...
      */

      if (count == 1)
        record = 0;

     /*
      * Try reading the data for the specified record, and validate the
      * contents...
      */

      expected = (ssize_t)sizeof(buffer) * record;

      if ((pos = cupsFileSeek(fp, expected)) != expected)
      {
        printf("FAIL (" CUPS_LLFMT " instead of " CUPS_LLFMT ")\n",
	       CUPS_LLCAST pos, CUPS_LLCAST expected);
        status ++;
	break;
      }
      else
      {
	if (cupsFileRead(fp, buffer, sizeof(buffer)) != sizeof(buffer))
	{
	  printf("FAIL (%s)\n", strerror(errno));
	  status ++;
	  break;
	}
	else if ((buffer[0] & 255) != (record & 255) ||
	         memcmp(buffer, buffer + 1, sizeof(buffer) - 1))
	{
	  printf("FAIL (Bad Data - %d instead of %d)\n", buffer[0] & 255,
	         record & 255);
	  status ++;
	  break;
	}
      }
    }

    if (count == 0)
      puts("PASS");

    cupsFileClose(fp);
  }

 /*
  * Remove the test file...
  */

  unlink("testfile.dat");

 /*
  * Return the test status...
  */

  return (status);
}


/*
 * 'read_write_tests()' - Perform read/write tests.
 */

static int				/* O - Status */
read_write_tests(int compression)	/* I - Use compression? */
{
  int		i;			/* Looping var */
  cups_file_t	*fp;			/* File */
  int		status;			/* Exit status */
  char		line[1024],		/* Line from file */
		*value;			/* Directive value from line */
  int		linenum;		/* Line number */
  unsigned char	readbuf[8192],		/* Read buffer */
		writebuf[8192];		/* Write buffer */
  int		byte;			/* Byte from file */
  ssize_t	bytes;			/* Number of bytes read/written */
  off_t		length;			/* Length of file */
  static const char *partial_line = "partial line";
					/* Partial line */


 /*
  * No errors so far...
  */

  status = 0;

 /*
  * Initialize the write buffer with random data...
  */

  CUPS_SRAND((unsigned)time(NULL));

  for (i = 0; i < (int)sizeof(writebuf); i ++)
    writebuf[i] = (unsigned char)CUPS_RAND();

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
      if (cupsFilePrintf(fp, "TestLine %03d\n", i) < 0)
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
    * cupsFileTell()
    */

    fputs("cupsFileTell(): ", stdout);

    if ((length = cupsFileTell(fp)) == 81933283)
      puts("PASS");
    else
    {
      printf("FAIL (" CUPS_LLFMT " instead of 81933283)\n", CUPS_LLCAST length);
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

  fputs("\ncupsFileOpen(read): ", stdout);

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

    for (i = 0, value = NULL; i < 1000; i ++)
      if (!cupsFileGetConf(fp, line, sizeof(line), &value, &linenum))
        break;
      else if (_cups_strcasecmp(line, "TestLine") || !value || atoi(value) != i ||
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

    for (i = 0, byte = 0; i < 256; i ++)
      if ((byte = cupsFileGetChar(fp)) != i)
        break;

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

    for (i = 0, bytes = 0; i < 10000; i ++)
      if ((bytes = cupsFileRead(fp, (char *)readbuf, sizeof(readbuf))) < 0)
        break;
      else if (memcmp(readbuf, writebuf, sizeof(readbuf)))
        break;

    if (i >= 10000)
      puts("PASS");
    else if (bytes > 0)
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

    for (i = 0; i < (int)strlen(partial_line); i ++)
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
    * cupsFileTell()
    */

    fputs("cupsFileTell(): ", stdout);

    if ((length = cupsFileTell(fp)) == 81933283)
      puts("PASS");
    else
    {
      printf("FAIL (" CUPS_LLFMT " instead of 81933283)\n", CUPS_LLCAST length);
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
  * Remove the test file...
  */

  if (!status)
    unlink(compression ? "testfile.dat.gz" : "testfile.dat");

 /*
  * Return the test status...
  */

  return (status);
}


/*
 * End of "$Id: testfile.c 12577 2015-03-30 19:07:17Z msweet $".
 */
