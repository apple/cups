/*
 * "$Id: dest.c,v 1.1 2000/01/26 00:03:25 mike Exp $"
 *
 *   User-defined destination (and option) support for the Common UNIX
 *   Printing System (CUPS).
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
 *   cupsAddDest()   - Add a destination to the list of destinations.
 *   cupsFreeDests() - Free the memory used by the list of destinations.
 *   cupsGetDest()   - Get the named destination from the list.
 *   cupsGetDests()  - Get the list of destinations.
 *   cupsSetDests()  - Set the list of destinations.
 */

/*
 * Include necessary headers...
 */

#include "cups.h"


/*
 * Local functions...
 */

static int	cups_get_dests(const char *filename, int num_dests,
		               cups_dest_t **dests);


/*
 * 'cupsAddDest()' - Add a destination to the list of destinations.
 */

int
cupsAddDest(const char  *name,
            int         num_dests,
            cups_dest_t **dests)
{
}


/*
 * 'cupsFreeDests()' - Free the memory used by the list of destinations.
 */

void
cupsFreeDests(int         num_dests,
              cups_dest_t **dests)
{
}


/*
 * 'cupsGetDest()' - Get the named destination from the list.
 */

cups_dest_t *
cupsGetDest(const char  *name,
            int         num_dests,
            cups_dest_t **dests)
{
}


/*
 * 'cupsGetDests()' - Get the list of destinations.
 */

int					/* O - Number of destinations */
cupsGetDests(cups_dest_t **dests)	/* O - Destinations */
{
  int		i;			/* Looping var */
  int		num_dests;		/* Number of destinations */
  int		count;			/* Number of printers/classes */
  char		**names;		/* Printer/class names */
  cups_dest_t	*dest;			/* Destination pointer */


 /*
  * Initialize destination array...
  */

  num_dests = 0;
  *dests    = (cups_dest_t *)0;

 /*
  * Grab all available printers...
  */

  if ((count = cupsGetPrinters(&names)) > 0)
  {
    for (i = 0; i < count; i ++)
    {
      num_dests = cupsAddDest(names[i], num_dests, dests);
      free(names[i]);
    }

    free(names);
  }

 /*
  * Grab all available classes...
  */
      
  if ((count = cupsGetClasses(&names)) > 0)
  {
    for (i = 0; i < count; i ++)
    {
      num_dests = cupsAddDest(names[i], num_dests, dests);
      free(names[i]);
    }

    free(names);
  }

 /*
  * Grab the default destination...
  */

  if ((dest = cupsGetDest(cupsGetDefault(), num_dests, *dests)) != NULL)
    dest->is_default = 1;

 /*
  * Load the /etc/cups/lpoptions and ~/.lpoptions files...
  */

  num_dests = cups_get_dests(

}


/*
 * 'cupsSetDests()' - Set the list of destinations.
 */

void
cupsSetDests(int         num_dests,
             cups_dest_t **dests)
{
}


/*
 * 'cups_get_dests()' - Get destinations from a file.
 */

static int				/* O - Number of destinations */
cups_get_dests(const char  *filename,	/* I - File to read from */
               int         num_dests,	/* I - Number of destinations */
               cups_dest_t **dests)	/* IO - Destinations */
{
}


/*
 * End of "$Id: dest.c,v 1.1 2000/01/26 00:03:25 mike Exp $".
 */
