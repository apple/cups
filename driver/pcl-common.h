/*
 * "$Id$"
 *
 *   Common HP-PCL definitions for CUPS.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1993-2005 by Easy Software Products, All Rights Reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include "data/pcl.h"


/*
 * Functions/macros...
 */

#define pcl_reset()\
	printf("\033E")
#define pcl_set_copies(copies)\
	printf("\033&l%dX", (copies))
#define pcl_set_pcl_mode(m)\
	printf("\033%%%dA", (m))
#define pcl_set_hpgl_mode(m)\
	printf("\033%%%dB", (m))
#define pcl_set_negative_motion()\
        printf("\033&a1N")
#define pcl_set_media_source(source)\
	printf("\033&l%dH", source)
#define pcl_set_media_type(type)\
	printf("\033&l%dM", type)
#define pcl_set_duplex(duplex,landscape)\
	if (duplex) printf("\033&l%dS", (duplex) + (landscape))
#define pcl_set_simple_black()\
	printf("\033*r-1U")
#define pcl_set_simple_color()\
	printf("\033*r3U")
#define pcl_set_simple_cmy()\
	printf("\033*r-3U")
#define pcl_set_simple_kcmy()\
	printf("\033*r-4U")
#define pcl_set_simple_resolution(r)\
	printf("\033*t%dR", (r))

#define pjl_escape()\
	printf("\033%%-12345X@PJL\r\n")
#define pjl_set_job(job_id,user,title)\
	printf("@PJL JOB NAME = \"%s\" DISPLAY = \"%d %s %s\"\r\n", \
	       (title), (job_id), (user), (title))
#define pjl_enter_language(lang)\
	printf("@PJL ENTER LANGUAGE=%s\r\n", (lang))

extern void	pcl_set_media_size(ppd_file_t *ppd, float width, float length);
extern void	pjl_write(ppd_file_t *ppd, const char *format,
		          const char *value, int job_id,
                	  const char *user, const char *title,
			  int num_options, cups_option_t *options);

/*
 * End of "$Id$".
 */
