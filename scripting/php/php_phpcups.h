/*
 * "$Id: php_phpcups.h,v 1.1.2.1 2002/08/19 19:42:18 mike Exp $"
 *
 *   PHP extensions for adding support for the Common UNIX
 *   Printing System (CUPS).
 *
 *   Copyright 1997-2002 by Easy Software Products.
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
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   Other functions and data structures in this file are generated 
 *   by the ext_skel script and are standard to PHP extensions. 
 *
 */
/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors:                                                             |
   |                                                                      |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_PHPCUPS_H
#define PHP_PHPCUPS_H

extern zend_module_entry phpcups_module_entry;
#define phpext_phpcups_ptr &phpcups_module_entry

#ifdef PHP_WIN32
#define PHP_PHPCUPS_API __declspec(dllexport)
#else
#define PHP_PHPCUPS_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(phpcups);
PHP_MSHUTDOWN_FUNCTION(phpcups);
PHP_RINIT_FUNCTION(phpcups);
PHP_RSHUTDOWN_FUNCTION(phpcups);
PHP_MINFO_FUNCTION(phpcups);

PHP_FUNCTION(confirm_phpcups_compiled);	
PHP_FUNCTION(phpcupsGetDestOptions);	
PHP_FUNCTION(phpcupsGetDests);	
PHP_FUNCTION(phpcupsGetJobs);	
PHP_FUNCTION(phpcupsCancelJob);	
PHP_FUNCTION(phpcupsPrintFile);	
PHP_FUNCTION(phpcupsLastError);	

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     

ZEND_BEGIN_MODULE_GLOBALS(phpcups)
	int   global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(phpcups)
*/

/* In every utility function you add that needs to use variables 
   in php_phpcups_globals, call TSRM_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMG_CC
   after the last function argument and declare your utility function
   with TSRMG_DC after the last declared argument.  Always refer to
   the globals in your function as PHPCUPS_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define PHPCUPS_G(v) TSRMG(phpcups_globals_id, zend_phpcups_globals *, v)
#else
#define PHPCUPS_G(v) (phpcups_globals.v)
#endif

#endif	/* PHP_PHPCUPS_H */


/*
 * End of "$Id: php_phpcups.h,v 1.1.2.1 2002/08/19 19:42:18 mike Exp $".
 */
