/*
 * common header for ipp everywhere printer commands for cups.
 *
 * copyright © 2019 by apple inc.
 *
 * licensed under apache license v2.0.  see the file "license" for more
 * information.
 */

/*
 * include necessary headers...
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cups/string-private.h>


/*
 * prototypes...
 */
