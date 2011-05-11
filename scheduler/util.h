/*
 * "$Id: util.h 7711 2008-07-02 04:39:27Z mike $"
 *
 *   Mini-daemon utility definitions for CUPS.
 *
 *   Copyright 2007-2011 by Apple Inc.
 *   Copyright 1997-2005 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

#ifndef _CUPSD_UTIL_H_
#  define _CUPSD_UTIL_H_

/*
 * Include necessary headers...
 */

#  include <cups/array-private.h>
#  include <cups/file-private.h>
#  include <signal.h>


/*
 * C++ magic...
 */

#  ifdef __cplusplus
extern "C" {
#  endif /* __cplusplus */


/*
 * Types...
 */

typedef int (*cupsd_compare_func_t)(const void *, const void *);


/*
 * Prototypes...
 */

extern int		cupsdCompareNames(const char *s, const char *t);
extern cups_array_t	*cupsdCreateStringsArray(const char *s);
extern int		cupsdExec(const char *command, char **argv);
extern cups_file_t	*cupsdPipeCommand(int *pid, const char *command,
			                  char **argv, int user);
extern void		cupsdSendIPPGroup(ipp_tag_t group_tag);
extern void		cupsdSendIPPHeader(ipp_status_t status_code,
			                   int request_id);
extern void		cupsdSendIPPInteger(ipp_tag_t value_tag,
			                    const char *name, int value);
extern void		cupsdSendIPPString(ipp_tag_t value_tag,
			                   const char *name, const char *value);
extern void		cupsdSendIPPTrailer(void);


#  ifdef __cplusplus
}
#  endif /* __cplusplus */

#endif /* !_CUPSD_UTIL_H_ */

/*
 * End of "$Id: util.h 7711 2008-07-02 04:39:27Z mike $".
 */
