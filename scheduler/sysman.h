/*
 * "$Id$"
 *
 *   System management definitions for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 */

/*
 * Constants...
 */

#define CUPSD_DIRTY_NONE	0	/* Nothing is dirty */
#define CUPSD_DIRTY_PRINTERS	1	/* printers.conf is dirty */
#define CUPSD_DIRTY_CLASSES	2	/* classes.conf is dirty */
#define CUPSD_DIRTY_REMOTE	4	/* remote.cache is dirty */
#define CUPSD_DIRTY_PRINTCAP	8	/* printcap is dirty */
#define CUPSD_DIRTY_JOBS	16	/* jobs.cache or "c" file(s) are dirty */
#define CUPSD_DIRTY_SUBSCRIPTIONS 32	/* subscriptions.conf is dirty */

/*
 * Globals...
 */

VAR int			DirtyFiles	VALUE(CUPSD_DIRTY_NONE),
					/* What files are dirty? */
			DirtyCleanInterval VALUE(60);
					/* How often do we write dirty files? */
VAR time_t		DirtyCleanTime	VALUE(0);
					/* When to clean dirty files next */
VAR int			Sleeping	VALUE(0);
					/* Non-zero if machine is entering or *
					 * in a sleep state...                */
#ifdef __APPLE__
VAR int			SysEventPipes[2] VALUE2(-1,-1);
					/* System event notification pipes */
#endif	/* __APPLE__ */


/*
 * Prototypes...
 */

extern void	cupsdCleanDirty(void);
extern void	cupsdMarkDirty(int what);
extern void	cupsdSetBusyState(void);
extern void	cupsdStartSystemMonitor(void);
extern void	cupsdStopSystemMonitor(void);
extern void	cupsdUpdateSystemMonitor(void);


/*
 * End of "$Id$".
 */
