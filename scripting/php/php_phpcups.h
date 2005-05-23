/*
 * "$Id$"
 *
 *   PHP module include file for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 1997-2003 by Easy Software Products.
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
 */

#ifndef PHP_PHPCUPS_H
#  define PHP_PHPCUPS_H

extern zend_module_entry phpcups_module_entry;
#  define phpext_phpcups_ptr &phpcups_module_entry

#  ifdef PHP_WIN32
#    define PHP_PHPCUPS_API __declspec(dllexport)
#  else
#    define PHP_PHPCUPS_API
#  endif

#  ifdef ZTS
#    include "TSRM.h"
#  endif

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

#  ifdef ZTS
#    define PHPCUPS_G(v) TSRMG(phpcups_globals_id, zend_phpcups_globals *, v)
#  else
#    define PHPCUPS_G(v) (phpcups_globals.v)
#  endif

#endif	/* !PHP_PHPCUPS_H */


/*
 * End of "$Id$".
 */
