/*
 * "$Id: md5-apple.h 6649 2007-07-11 21:46:42Z mike $"
 *
 *   MD5 MacOS X compatibility header for the Common UNIX Printing
 *   System (CUPS).
 *
 *   This file just defines aliases to the (private) CUPS MD5 functions.
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 */

void md5_init(_cups_md5_state_t *pms)
     { _cupsMD5Init(pms); }
void md5_append(_cups_md5_state_t *pms, const unsigned char *data, int nbytes)
     { _cupsMD5Append(pms, data, nbytes); }
void md5_finish(_cups_md5_state_t *pms, unsigned char digest[16])
     { _cupsMD5Finish(pms, digest); }

/*
 * End of "$Id: md5-apple.h 6649 2007-07-11 21:46:42Z mike $".
 */
