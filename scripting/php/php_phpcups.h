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
PHP_FUNCTION(cups_get_dest_options);	
PHP_FUNCTION(cups_get_dest_list);	
PHP_FUNCTION(cups_get_jobs);	
PHP_FUNCTION(cups_cancel_job);	
PHP_FUNCTION(cups_print_file);	
PHP_FUNCTION(cups_last_error);	
PHP_FUNCTION(cups_get_printer_attributes);	

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
 */
