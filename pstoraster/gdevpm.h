/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gdevpm.h */
/* Defines common to gdevpm.c, gspmdrv.c and PM GSview */

#define SHARED_NAME "\\SHAREMEM\\%s"
#define SYNC_NAME   "\\SEM32\\SYNC_%s"
#define NEXT_NAME   "\\SEM32\\NEXT_%s"
#define MUTEX_NAME  "\\SEM32\\MUTEX_%s"
#define QUEUE_NAME  "\\QUEUES\\%s"

#define GS_UPDATING	1
#define GS_SYNC		2
#define GS_PAGE		3
#define GS_CLOSE	4
#define GS_ERROR	5
#define GS_PALCHANGE	6
#define GS_BEGIN	7
#define GS_END		8
