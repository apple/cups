/*
 * "$Id: ieee1394.h,v 1.1 2002/05/04 16:58:23 mike Exp $"
 *
 *   IEEE-1394 header for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2002 by Easy Software Products, all rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *	  copyright notice, this list of conditions and the
 *	  following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the
 *	  above copyright notice, this list of conditions and
 *	  the following disclaimer in the documentation and/or
 *	  other materials provided with the distribution.
 *
 *     3. All advertising materials mentioning features or use
 *	  of this software must display the following
 *	  acknowledgement:
 *
 *	    This product includes software developed by Easy
 *	    Software Products.
 *
 *     4. The name of Easy Software Products may not be used to
 *	  endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS
 *   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *   BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *   DAMAGE.
 */

/*
 * Include necessary headers.
 */

#include <cups/cups.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cups/string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef WIN32
#  include <io.h>
#else
#  include <unistd.h>
#  include <fcntl.h>
#endif /* WIN32 */


/*
 * Device information structure...
 */

typedef struct
{
  char	uri[HTTP_MAX_URI],	/* Device URI */
	description[128],	/* Description of port */
	make_model[128];	/* Make and model */
} ieee1394_info_t;


/*
 * Private device connection information...
 */

typedef void	*ieee1394_dev_t;


/*
 * Prototypes for standard IEEE-1394 interface...
 */

extern ieee1394_info_t	*ieee1394_list(int *num_devices);
extern ieee1394_dev_t	ieee1394_open(const char *uri);
extern int		ieee1394_close(ieee1394_dev_t dev);
extern int		ieee1394_read(ieee1394_dev_t dev, char *buffer, int len);
extern int		ieee1394_write(ieee1394_dev_t dev, char *buffer, int len);
extern const char	*ieee1394_error(void);


/*
 * End of "$Id: ieee1394.h,v 1.1 2002/05/04 16:58:23 mike Exp $".
 */
