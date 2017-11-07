/*
 * CGI test program for CUPS.
 *
 * Copyright 2007-2014 by Apple Inc.
 * Copyright 1997-2005 by Easy Software Products.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more information.
 */

/*
 * Include necessary headers...
 */

#include "cgi.h"


/*
 * 'main()' - Test the CGI code.
 */

int					/* O - Exit status */
main(void)
{
 /*
  * Test file upload/multi-part submissions...
  */

  freopen("multipart.dat", "rb", stdin);

  putenv("CONTENT_TYPE=multipart/form-data; "
         "boundary=---------------------------1977426492562745908748943111");
  putenv("REQUEST_METHOD=POST");

  printf("cgiInitialize: ");
  if (cgiInitialize())
  {
    const cgi_file_t	*file;		/* Upload file */

    if ((file = cgiGetFile()) != NULL)
    {
      puts("PASS");
      printf("    tempfile=\"%s\"\n", file->tempfile);
      printf("    name=\"%s\"\n", file->name);
      printf("    filename=\"%s\"\n", file->filename);
      printf("    mimetype=\"%s\"\n", file->mimetype);
    }
    else
      puts("FAIL (no file!)");
  }
  else
    puts("FAIL (init)");

 /*
  * Return with no errors...
  */

  return (0);
}
