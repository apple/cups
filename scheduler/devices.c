/*
 * "$Id: devices.c,v 1.2 2000/02/10 00:57:54 mike Exp $"
 *
 *   Device scanning routines for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2000 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   LoadDevices() - Load all available devices.
 */

/*
 * Include necessary headers...
 */

#include "cupsd.h"

#if HAVE_DIRENT_H
#  include <dirent.h>
typedef struct dirent DIRENT;
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
typedef struct direct DIRENT;
#  define NAMLEN(dirent) (dirent)->d_namlen
#endif


/*
 * 'LoadDevices()' - Load all available devices.
 */

void
LoadDevices(const char *d)	/* I - Directory to scan */
{
  FILE		*fp;		/* Pipe to device backend */
  DIR		*dir;		/* Directory pointer */
  DIRENT	*dent;		/* Directory entry */
  char		filename[1024],	/* Name of backend */
		line[2048],	/* Line from backend */
		dclass[64],	/* Device class */
		uri[1024],	/* Device URI */
		info[128],	/* Device info */
		make_model[256];/* Make and model */


  Devices = ippNew();

  ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
               "device-class", NULL, "file");
  ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "device-info", NULL, "Disk File");
  ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_TEXT,
               "device-make-and-model", NULL, "Unknown");
  ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_URI,
               "device-uri", NULL, "file");

  if ((dir = opendir(d)) == NULL)
  {
    LogMessage(L_ERROR, "LoadDevices: Unable to open backend directory \"%s\": %s",
               d, strerror(errno));
    return;
  }

  while ((dent = readdir(dir)) != NULL)
  {
   /*
    * Skip "." and ".."...
    */

    if (dent->d_name[0] == '.')
      continue;

   /*
    * Run the backend with no arguments and collect the output...
    */

    snprintf(filename, sizeof(filename), "%s/%s", d, dent->d_name);
    if ((fp = popen(filename, "r")) != NULL)
    {
      while (fgets(line, sizeof(line), fp) != NULL)
      {
       /*
        * Each line is of the form:
	*
	*   class URI "make model" "name"
	*/

        if (sscanf(line, "%63s%1023s%*[ \t]\"%127[^\"]\"%*[ \t]\"%255[^\"]",
	           dclass, uri, make_model, info) != 4)
        {
	 /*
	  * Bad format; strip trailing newline and write an error message.
	  */

	  line[strlen(line) - 1] = '\0';
	  LogMessage(L_ERROR, "LoadDevices: Bad line from \"%s\": %s",
	             dent->d_name, line);
        }
	else
	{
	 /*
	  * Add strings to attributes...
	  */

          ippAddSeparator(Devices);
          ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_KEYWORD,
                       "device-class", NULL, dclass);
          ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "device-info", NULL, info);
          ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_TEXT,
                       "device-make-and-model", NULL, make_model);
          ippAddString(Devices, IPP_TAG_PRINTER, IPP_TAG_URI,
                       "device-uri", NULL, uri);

          LogMessage(L_DEBUG, "LoadDevices: Adding device \"%s\"...", uri);
	}
      }

      pclose(fp);
    }
    else
      LogMessage(L_WARN, "LoadDevices: Unable to execute \"%s\" backend: %s",
                 dent->d_name, strerror(errno));
  }

  closedir(dir);
}


/*
 * End of "$Id: devices.c,v 1.2 2000/02/10 00:57:54 mike Exp $".
 */
