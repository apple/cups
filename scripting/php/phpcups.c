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

/*
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phpcups.h"

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ipp.h>
#include <cups/language.h>
#include <cups/string.h>
#include <cups/debug.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(WIN32) || defined(__EMX__)
#  include <io.h>
#else
#  include <unistd.h>
#endif /* WIN32 || __EMX__ */


static int le_result, le_link, le_plink;




/*
 * Local globals...
 */

static http_t		*cups_server = NULL;	/* Current server connection */
static ipp_status_t	last_error = IPP_OK;	/* Last IPP error */
static char		authstring[HTTP_MAX_VALUE] = "";
						/* Authorization string */
static char		pwdstring[33] = "";	/* Last password string */

typedef struct printer_attrs_type
{
  char      *name;
  char      *value;
} printer_attrs_t;


int              num_attrs = 0;
printer_attrs_t  *printer_attrs = NULL;

int  _phpcups_get_printer_status(char *name );
void free_attrs_list(void);
void _phpcups_free_attrs_list(void);





/*
 *  *******************************************************************
 *
 *  CUPS prototypes from the file cups/util.c of the cups distribution.
 *
 *  *******************************************************************
 */


int				/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,	/* I - Name of printer or class */
              int        job);	/* I - Job ID */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - HTTP connection to server */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename);	/* I - File to send or NULL */
void
cupsFreeJobs(int        num_jobs,/* I - Number of jobs */
             cups_job_t *jobs);	 /* I - Jobs */

int				        /* O - Number of classes */
cupsGetClasses(char ***classes);	/* O - Classes */

const char *			/* O - Default printer or NULL */
cupsGetDefault(void);


int					/* O - Number of jobs */
cupsGetJobs(cups_job_t **jobs,		/* O - Job data */
            const char *mydest,		/* I - Only show jobs for dest? */
            int        myjobs,		/* I - Only show my jobs? */
	    int        completed);	/* I - Only show completed jobs? */

const char *				/* O - Filename for PPD file */
cupsGetPPD(const char *name);		/* I - Printer name */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers);	/* O - Printers */


ipp_status_t		/* O - IPP error code */
cupsLastError(void);

int					/* O - Job ID */
cupsPrintFile(const char    *name,	/* I - Printer or class name */
              const char    *filename,	/* I - File to print */
	      const char    *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options);	/* I - Options */


int					/* O - Job ID */
cupsPrintFiles(const char    *name,	/* I - Printer or class name */
               int           num_files,	/* I - Number of files */
               const char    **files,	/* I - File(s) to print */
	       const char    *title,	/* I - Title of job */
               int           num_options,/* I - Number of options */
	       cups_option_t *options);	/* I - Options */


static char *				/* I - Printer name or NULL */
cups_connect(const char *name,		/* I - Destination (printer[@host]) */
	     char       *printer,	/* O - Printer name [HTTP_MAX_URI] */
             char       *hostname);	/* O - Hostname [HTTP_MAX_URI] */

static int			/* O - 1 if available, 0 if not */
cups_local_auth(http_t *http);	/* I - Connection */

/*
 *  *********************************************************************
 */




/* If you declare any globals in php_phpcups.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(phpcups)
*/

/* True global resources - no need for thread safety here */
static int le_phpcups;

/* 
 * Every user visible function must have an entry in phpcups_functions[].
 */
function_entry phpcups_functions[] = {
	PHP_FE(confirm_phpcups_compiled,NULL)
	PHP_FE(cups_get_dest_list,	NULL)		
	PHP_FE(cups_get_dest_options,	NULL)		
	PHP_FE(cups_get_jobs,	NULL)		
	PHP_FE(cups_cancel_job,	NULL)		
	PHP_FE(cups_last_error,	NULL)		
	PHP_FE(cups_print_file,	NULL)		
	PHP_FE(cups_get_printer_attributes,	NULL)		
	{NULL, NULL, NULL}	
};




zend_module_entry phpcups_module_entry = {
	STANDARD_MODULE_HEADER,
	"phpcups",
	phpcups_functions,
	PHP_MINIT(phpcups),
	PHP_MSHUTDOWN(phpcups),
	PHP_RINIT(phpcups),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(phpcups),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(phpcups),
    "0.1", /* Replace with version number for your extension */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHPCUPS
ZEND_GET_MODULE(phpcups)
#endif




/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phpcups.value",      "42", PHP_INI_ALL, OnUpdateInt, global_value, zend_phpcups_globals, phpcups_globals)
    STD_PHP_INI_ENTRY("phpcups.string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_phpcups_globals, phpcups_globals)
PHP_INI_END()
*/




/* Uncomment this function if you have INI entries
static void php_phpcups_init_globals(zend_phpcups_globals *phpcups_globals)
{
	phpcups_globals->value = 0;
	phpcups_globals->string = NULL;
}
*/




PHP_MINIT_FUNCTION(phpcups)
{
	/* If you have INI entries, uncomment these lines 
	ZEND_INIT_MODULE_GLOBALS(phpcups, php_phpcups_init_globals, NULL);
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phpcups)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}

/* Remove if there's nothing to do at request start */
PHP_RINIT_FUNCTION(phpcups)
{
	return SUCCESS;
}

/* Remove if there's nothing to do at request end */
PHP_RSHUTDOWN_FUNCTION(phpcups)
{
	return SUCCESS;
}



PHP_MINFO_FUNCTION(phpcups)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "phpcups support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}







/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
PHP_FUNCTION(confirm_phpcups_compiled)
{
	char *arg = NULL;
	int arg_len, len;
	char string[256];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	len = sprintf(string, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "phpcups", arg);
	RETURN_STRINGL(string, len, 1);
}
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/











/*
 *  Function:    cups_get_dest_options
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  name     - String  - Name of destination.
 *               instance - String  - Name of instance on destination.
 *
 *  Returns:     Array of option "objects", with each object
 *               containing the members:
 *
 *               name  - String - Option name
 *               value - String - Option value
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_get_dest_options)
{
    char        *arg = NULL;
    int         arg_len, len;

    zval        *new_object;

    zval        **d_server,
                **d_name, 
                **d_instance;

    char	c_server[256],
                c_name[256], 
                c_instance[256];

    char	l_server[256],
                l_name[256], 
                l_instance[256];

    cups_dest_t *dests, *dptr;
    int         num_dests;
    int         i, j;

    array_init(return_value);

    if ((ZEND_NUM_ARGS() != 3) ||
        (zend_get_parameters_ex(3,&d_server,&d_name,&d_instance) != SUCCESS))
    {
      WRONG_PARAM_COUNT;
    }

    convert_to_string_ex( d_server );
    convert_to_string_ex( d_name );
    convert_to_string_ex( d_instance );

    /*
     *  Find the dest/instance we want options for.
     */
    bzero( c_server, 256 );
    bzero( c_name, 256 );
    bzero( c_instance, 256 );

    if ( (char *)(*d_server)->value.str.val != NULL )
      strcpy( c_server,(char *)(*d_server)->value.str.val );
    if ( (char *)(*d_name)->value.str.val != NULL )
      strcpy( c_name,(char *)(*d_name)->value.str.val );
    if ( (char *)(*d_instance)->value.str.val != NULL )
      strcpy( c_instance,(char *)(*d_instance)->value.str.val );
      

    if (strlen(c_server))
      cupsSetServer(c_server);

    num_dests = cupsGetDests(&dests); 
    for (i=0, j = -1; (i < num_dests) && (j < 0); i++)
    {
      dptr = &dests[i];

      if (dptr->name == NULL)
        strcpy( l_name, "" );
      else
	    strcpy( l_name, dptr->name );

      if (dptr->instance == NULL)
	    strcpy( l_instance, "" );
      else
        strcpy( l_instance, dptr->instance );

      if ((!strcmp( l_name, c_name )) &&
          (!strcmp( l_instance, c_instance )))
      {

        for (j=0; j < dptr->num_options; j++ )
        {
          if ((dptr->options[j].name != NULL) &&
              (dptr->options[j].value != NULL))
         {
            MAKE_STD_ZVAL(new_object);
            if (object_init(new_object) == SUCCESS)
            {
              add_property_string(new_object,"name",dptr->options[j].name, 1 );
              add_property_string(new_object,"value",dptr->options[j].value,1);
              add_next_index_zval( return_value, new_object );
            }
          }
        }
      }
    }
    cupsFreeDests(num_dests,dests); 
}




/*
 *  Function:    cups_get_dest_list
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  cups server (optional)
 *
 *  Returns:     Array of destination "objects", with each object
 *               containing the members:
 *
 *                 name        - String - Name of destination.
 *                 instance    - String - Name of instance on destination.
 *                 is_default  - Long   - 1 if default printer.
 *                 num_options - Long   - Number of options for destination.
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_get_dest_list)
{
    char        *arg = NULL;
    int         arg_len, len;

    zval        **z_server;
    zval        *new_object;

    char        c_server[256];

    char        string[2560];
    char        temp[256];

    cups_dest_t *dests, *dptr;
    int         num_dests;
    int         i;

    /*
     *  Initialize the return array.
     */
    array_init(return_value);

    switch (ZEND_NUM_ARGS())
    {
      /*
       *  Change servers if passed.
       */
      case 1: if (zend_get_parameters_ex(1,&z_server) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server );
              if ( (char *)(*z_server)->value.str.val != NULL )
              {
                strcpy( c_server,(char *)(*z_server)->value.str.val );
                cupsSetServer( c_server );
              }
              break;
    }


    /*
     *  First get the destination list from the cups server.
     */
    num_dests = cupsGetDests(&dests); 

    /*
     *  Loop through the list, create and fill in the objects, and
     *  add them to the array.
     */
    string[0] = '\0';
    for (i=0; i < num_dests; i++)
    {
      dptr = &dests[i];

      MAKE_STD_ZVAL(new_object);
      if (object_init(new_object) == SUCCESS)
      {
        if (strlen(c_server))
          add_property_string( new_object, "server", c_server, 1 );
        else
          add_property_string( new_object, "server", "", 1 );

        if (dptr->name != NULL)
          add_property_string( new_object, "name", dptr->name, 1 );
        else
          add_property_string( new_object, "name", "", 1 );

        if (dptr->instance != NULL)
          add_property_string( new_object, "instance", dptr->instance, 1 );
        else
          add_property_string( new_object, "instance", "", 1 );

        add_property_long( new_object, "is_default", dptr->is_default );
        add_property_long( new_object, "num_options", dptr->num_options );
        add_next_index_zval( return_value, new_object );
      }
    }

    /*
     *  free the list .....
     */
    cupsFreeDests(num_dests,dests); 
}



/*
 *  Function:    cups_get_jobs
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  server    - String  - Name or IP of cups server.  Blank
 *                                     for localhost.
 *
 *               name      - String  - Name of destination to query.
 *
 *               user      - String  - Username to get job list for.
 *                         * Optional, default all users
 *
 *               my_jobs   - Long    - Show only my jobs
 *                         * Optional, default FALSE
 *
 *               completed - Long    - Show completed jobs
 *                         * Optional, default FALSE
 *
 *  Returns:     Array of print job "objects", with each object
 *               containing the members:
 *
 *               id             - Long    - Job id
 *               dest           - String  - Name of destination.
 *               title          - String  - Title of document.
 *               user           - String  - User who submitted job.
 *               format         - String  - Document format (MIME type)
 *               state          - Long    - Current state of the job.
 *               size           - Long    - Size in bytes of the job.
 *               priority       - Long    - Job priority.
 *               completed_time - Long    - Time job completed (UNIX time).
 *               creation_time  - Long    - Time job created (UNIX time).
 *               processing_time- Long    - Processing time in seconds?
 *
 *  Comments:
 *
 */
PHP_FUNCTION( cups_get_jobs )
{
    char        *arg = NULL;

    int         arg_len, 
                len;

    zval        *new_object;

    zval        **z_server,
                **z_name, 
                **z_user,
                **z_myjobs, 
                **z_completed;

    char	p_server[256],
                p_name[256],
                p_user[256];

    int         p_myjobs, 
                p_completed;

    cups_job_t  *jobs, 
                *jptr;

    int         num_jobs;
    int         i;

    bzero( p_server, 256 );
    bzero( p_name, 256 );
    bzero( p_user, 256 );
    p_myjobs    = 0;
    p_completed = 0;

    /*
     *  Initialize return value.
     */
    array_init(return_value);

    /*
     *  Parse params.
     */
    switch(ZEND_NUM_ARGS())
    {
      /*
       *  server, destination only provided.
       */
      case 2: 
              if (zend_get_parameters_ex( 2, &z_server,&z_name ) == SUCCESS)
              {
                convert_to_string_ex( z_server);
                if ( (char *)(*z_server)->value.str.val != NULL )
                  strcpy( p_server,(char *)(*z_server)->value.str.val );
                convert_to_string_ex( z_name );
                if ( (char *)(*z_name)->value.str.val != NULL )
                  strcpy( p_name,(char *)(*z_name)->value.str.val );
              }
              break;

      /*
       *  server, destination and user
       */
      case 3: 
              if (zend_get_parameters_ex( 3, &z_server, &z_name, 
                                             &z_user ) == SUCCESS)
              {
                convert_to_string_ex( z_name );
                convert_to_string_ex( z_user );
                convert_to_string_ex( z_server);
                if ( (char *)(*z_server)->value.str.val != NULL )
                  strcpy( p_server,(char *)(*z_server)->value.str.val );
                if ( (char *)(*z_name)->value.str.val != NULL )
                  strcpy( p_name,(char *)(*z_name)->value.str.val );
                if ( (char *)(*z_user)->value.str.val != NULL )
                  strcpy( p_user,(char *)(*z_user)->value.str.val );
              }
              break;

      /*
       *  server, destination, user, and myjobs
       */
      case 4: 
              if (zend_get_parameters_ex( 4, &z_server, &z_name, &z_user, 
                                             &z_myjobs ) == SUCCESS)
              {
                convert_to_string_ex( z_name );
                convert_to_string_ex( z_user );
                convert_to_string_ex( z_server);
                if ( (char *)(*z_server)->value.str.val != NULL )
                  strcpy( p_server,(char *)(*z_server)->value.str.val );
                if ( (char *)(*z_name)->value.str.val != NULL )
                  strcpy( p_name,(char *)(*z_name)->value.str.val );
                if ( (char *)(*z_user)->value.str.val != NULL )
                  strcpy( p_user,(char *)(*z_user)->value.str.val );

                convert_to_string_ex( z_myjobs );
                p_myjobs = strtoul((char *)(*z_myjobs)->value.str.val,NULL,10);
              }
              break;

      /*
       *  server, destination, user, myjobs, and completed
       */
      case 5: 
              if (zend_get_parameters_ex( 5, &z_server, &z_name, &z_user, 
                                          &z_myjobs, &z_completed ) == SUCCESS)
              {
                convert_to_string_ex( z_name );
                convert_to_string_ex( z_user );
                convert_to_string_ex( z_server);
                if ( (char *)(*z_server)->value.str.val != NULL )
                  strcpy( p_server,(char *)(*z_server)->value.str.val );
                if ( (char *)(*z_name)->value.str.val != NULL )
                  strcpy( p_name,(char *)(*z_name)->value.str.val );
                if ( (char *)(*z_user)->value.str.val != NULL )
                  strcpy( p_user,(char *)(*z_user)->value.str.val );

                convert_to_string_ex( z_myjobs );
                p_myjobs = strtoul((char *)(*z_myjobs)->value.str.val,NULL,10);

                convert_to_string_ex( z_completed);
                p_completed = strtoul((char *)(*z_completed)->value.str.val,NULL,10);
              }
              break;
    }

    /*
     *  Set the cups server if given.
     */
    if (strlen(p_server))
      cupsSetServer(p_server);

    /*
     *  Set the cups user if given, otherwise get all jobs.
     */
    if (strlen(p_user))
      cupsSetUser(p_user); 
    else
      cupsSetUser("root"); 

    /*
     *  Get the jobs list from the CUPS server.
     */
    num_jobs = cupsGetJobs(&jobs,p_name,p_myjobs,p_completed); 

    /*
     *  Build the array of objects to return.
     */
    for (i=0; i < num_jobs; i++)
    {
      jptr = &jobs[i];

      MAKE_STD_ZVAL(new_object);
      if (object_init(new_object) == SUCCESS)
      {
        add_property_long(new_object,"id",jptr->id  );
        add_property_string(new_object,"dest",jptr->dest, 1 );
        add_property_string(new_object,"title",jptr->title, 1 );
        add_property_string(new_object,"user",jptr->user, 1 );
        add_property_string(new_object,"format",jptr->format, 1 );
        add_property_long(new_object,"state",jptr->state );
        add_property_long(new_object,"size",jptr->size );
        add_property_long(new_object,"priority",jptr->priority );
        add_property_long(new_object,"completed_time",jptr->completed_time);
        add_property_long(new_object,"creation_time",jptr->creation_time);
        add_property_long(new_object,"processing_time",jptr->processing_time);
        add_next_index_zval( return_value, new_object );
      }
      else
      {
        printf("\nError creating new object\n");
      }
    }
    cupsFreeJobs(num_jobs,jobs); 
}




/*
 *  Function:    cups_last_error
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  none.
 *
 *  Returns:     error number - Long
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_last_error)
{
  static char error[1024];

  zval **z_server;
  char c_server[256];

  bzero( c_server, 256 );

  switch (ZEND_NUM_ARGS())
  {
      /*
       *  Change servers if passed.
       */
      case 1: if (zend_get_parameters_ex(1,&z_server) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server );
              if ( (char *)(*z_server)->value.str.val != NULL )
              {
                strcpy( c_server,(char *)(*z_server)->value.str.val );
                cupsSetServer( c_server );
              }
              break;
  }
  sprintf( error,"%d", cupsLastError());

  RETURN_STRINGL(error, strlen(error)+1, 1);
}






/*
 *  Function:    cups_cancel_job
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  server - String  -  Name or IP of cups server.
 *               name   - String  -  Name of destination.
 *               job    - Long    -  Job ID to cancel.
 *
 *  Returns:     1 on success, 0 on failure.
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_cancel_job)
{
    zval        **z_server, **z_name, **z_job;

    char	p_server[256], p_name[256];
    int         p_job;

    int         ret_val = -1;

    /*
     *  Get parameters .....
     */
    if ((ZEND_NUM_ARGS() != 3) ||
        (zend_get_parameters_ex( 3, &z_server, &z_name, &z_job ) != SUCCESS))
    {
      WRONG_PARAM_COUNT;
    }

    convert_to_string_ex( z_server);
    if ( (char *)(*z_name)->value.str.val != NULL )
    {
      strcpy( p_server,(char *)(*z_server)->value.str.val );
      cupsSetServer(p_server);
    }

    convert_to_string_ex( z_name );
    if ( (char *)(*z_name)->value.str.val != NULL )
      strcpy( p_name,(char *)(*z_name)->value.str.val );

    convert_to_string_ex( z_job );
    p_job = strtoul((char *)(*z_job)->value.str.val,NULL,10);

    if (strlen(p_server))
      cupsSetServer(p_server);
    cupsSetUser("root");

    /*
     *  Errrr ....  Cancel the job ......
     */
    ret_val = cupsCancelJob(p_name,p_job);

    RETURN_LONG(ret_val);
}



/*
 *  Local function.
 */
cups_option_t *_phpcups_parse_options( cups_option_t *options, 
                                       int *num_options, char *param )
{
  char name[1024], value[1024];

  sscanf(param,"%1023[^=]=%1023s", name, value ); 

  if (strlen(name) && strlen(value))
  {
    if (options == NULL)
    {
      options = (cups_option_t *)emalloc(sizeof(cups_option_t));
      options->name = (char *)emalloc(strlen(name)+1);
      options->value = (char *)emalloc(strlen(value)+1);
      strcpy( options->name, name );
      strcpy( options->value, value );
      *num_options++;
    }
    else
    {
      options = (cups_option_t *)erealloc(options,
                                  (*num_options+1) * sizeof(cups_option_t));
      options[*num_options].name = (char *)emalloc(strlen(name)+1);
      options[*num_options].value = (char *)emalloc(strlen(value)+1);
      strcpy( options[*num_options].name, name );
      strcpy( options[*num_options].value, value );
      *num_options++;
    }
  }
  return(options);
}


/*
 *  Function:    cups_print_file
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  printer    - String  -  Name of destination.
 *               filename   - String  -  Name of file to print (full path).
 *               title      - String  -  Title of document.
 *
 *  Returns:     Job ID     - Long
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_print_file)
{
    zval        **z_server, **z_printer, **z_filename, **z_title, 
                **z_options, **keydata;

    HashTable   *param_ht;

    char	p_server[256];
    char	p_printer[256];
    char	p_filename[256];
    char	p_title[256];

    char           temp[4096];
    cups_option_t  *options = NULL;

    int         count, current;
    int         ret_val = -1;


    int zend_num_args = ZEND_NUM_ARGS();
    switch (zend_num_args) 
    {
      /*
       *  Server / Destination / filename only.
       */
      case 3:
              if (zend_get_parameters_ex( 3, &z_server, &z_printer, 
                                             &z_filename) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server);
              if ( (char *)(*z_server)->value.str.val != NULL )
              {
                strcpy( p_server,(char *)(*z_server)->value.str.val );
                cupsSetServer(p_server);
              }
              convert_to_string_ex( z_printer);
              if ( (char *)(*z_printer)->value.str.val != NULL )
                strcpy( p_printer,(char *)(*z_printer)->value.str.val );
              convert_to_string_ex( z_filename );
              if ( (char *)(*z_filename)->value.str.val != NULL )
                strcpy( p_filename,(char *)(*z_filename)->value.str.val );
              strcpy( p_title,"untitled");
              break;

      /*
       *  Server, destination, filename and title.
       */
      case 4:
              if (zend_get_parameters_ex( 4, &z_server,
                                             &z_printer, 
                                             &z_filename, 
                                             &z_title) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server);
              if ( (char *)(*z_server)->value.str.val != NULL )
              {
                strcpy( p_server,(char *)(*z_server)->value.str.val );
                cupsSetServer(p_server);
              }
              convert_to_string_ex( z_printer);
              if ( (char *)(*z_printer)->value.str.val != NULL )
                strcpy( p_printer,(char *)(*z_printer)->value.str.val );
              convert_to_string_ex( z_filename );
              if ( (char *)(*z_filename)->value.str.val != NULL )
                strcpy( p_filename,(char *)(*z_filename)->value.str.val );
              convert_to_string_ex( z_title );
              if ( (char *)(*z_title)->value.str.val != NULL )
                strcpy( p_title,(char *)(*z_title)->value.str.val );
              break;

      /*
       *  Server, destination, filename and title plus printer options.
       */
      case 5:
              if (zend_get_parameters_ex( 5, &z_server,
                                             &z_printer, 
                                             &z_filename, 
                                             &z_title,
                                             &z_options) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server);
              if ( (char *)(*z_server)->value.str.val != NULL )
              {
                strcpy( p_server,(char *)(*z_server)->value.str.val );
                cupsSetServer(p_server);
              }
              convert_to_string_ex( z_printer);
              if ( (char *)(*z_printer)->value.str.val != NULL )
                strcpy( p_printer,(char *)(*z_printer)->value.str.val );
              convert_to_string_ex( z_filename );
              if ( (char *)(*z_filename)->value.str.val != NULL )
                strcpy( p_filename,(char *)(*z_filename)->value.str.val );
              convert_to_string_ex( z_title );
              if ( (char *)(*z_title)->value.str.val != NULL )
                strcpy( p_title,(char *)(*z_title)->value.str.val );

              /*
               *  Convert options array.
               */
              convert_to_array_ex( z_options );
              param_ht = Z_ARRVAL_PP( z_options );
              count    = zend_hash_num_elements( param_ht );
              options  = emalloc(sizeof(zval **) * count);

              current = 0;
              zend_hash_internal_pointer_reset(param_ht);
              for (current=0; current < count; current++)
              {
                zend_hash_get_current_data(param_ht,(void **)&keydata);   
                switch(Z_TYPE_PP(keydata))
                {
                  case IS_STRING:
                         convert_to_string_ex(keydata);
                         strcpy(temp,(char *)(*keydata)->value.str.val );
                         options = _phpcups_parse_options( options, &current,
                                                           temp );
                         break;
                }
                zend_hash_move_forward(param_ht);
              }
              break;

      default: WRONG_PARAM_COUNT;
    }

    if (current > 0)
    {
      ret_val = cupsPrintFile( p_printer,p_filename,p_title,current,options);
      for (current=0; current < count; current++)
      {
        efree( options[current].name );
        efree( options[current].value );
      }
      efree(options);
    }
    else
      ret_val = cupsPrintFile( p_printer,p_filename,p_title,0,NULL );

    RETURN_LONG(ret_val);
}







/*
 *  Function:    cups_get_printer_attributes
 *
 *  Date:        8 Nov 2002 - TDB
 *
 *  Parameters:  name   - String  -  Name of destination.
 *
 *  Returns:     state  - String
 *
 *  Comments:
 *
 */
PHP_FUNCTION(cups_get_printer_attributes)
{
    zval    **z_server, **z_name;

    zval    *new_object;

    char    p_server[256], p_name[256];

    int     i, count;


    /*
     *  Initialize return value.
     */
    array_init(return_value);

    /*
     *  Get parameters .....
     */
    if ((ZEND_NUM_ARGS() != 2) ||
        (zend_get_parameters_ex( 2, &z_server, &z_name ) != SUCCESS))
    {
      WRONG_PARAM_COUNT;
    }

    convert_to_string_ex( z_server);
    if ( (char *)(*z_server)->value.str.val != NULL )
    {
      strcpy( p_server,(char *)(*z_server)->value.str.val );
      cupsSetServer(p_server);
    }
    convert_to_string_ex( z_name );
    if ( (char *)(*z_name)->value.str.val != NULL )
      strcpy( p_name,(char *)(*z_name)->value.str.val );

    /*
     *  Errrr ....  Cancel the job ......
     */
    printer_attrs = NULL;

    count = _phpcups_get_printer_status( p_name );

    /*
     *  Create an array of objects containing name / value pairs.
     */
    for (i=0; i < count; i++)
    {
      if ((printer_attrs[i].name != NULL) && (printer_attrs[i].value != NULL))
      {
        MAKE_STD_ZVAL(new_object);
        if (object_init(new_object) == SUCCESS)
        {
          add_property_string(new_object,"name",printer_attrs[i].name, 1 );
          add_property_string(new_object,"value",printer_attrs[i].value, 1 );
          add_next_index_zval( return_value, new_object );
        }
      }
    }
    _phpcups_free_attrs_list();
}




/*
 *  Local function - free memory.
 */
void _phpcups_free_attrs_list(void)
{
  int i;

  if (num_attrs < 1)
    return;

  for ( i=0; i < num_attrs; i++ )
  {
      if (printer_attrs[i].name != NULL)
        efree(printer_attrs[i].name);
      if (printer_attrs[i].value != NULL)
        efree(printer_attrs[i].value );
  }
  efree(printer_attrs);
  num_attrs = 0;
}







/*
 *  Local function - add to attributes list.
 */
int _phpcups_update_attrs_list( char *name, char *value )
{
  if (num_attrs < 1)
  {
    printer_attrs = (printer_attrs_t *)emalloc(sizeof(printer_attrs_t));
    printer_attrs->name = (char *)emalloc(strlen(name)+1);
    printer_attrs->value = (char *)emalloc(strlen(value)+1);
    strcpy( printer_attrs->name, name );
    strcpy( printer_attrs->value, value );
    num_attrs = 1;
  }
  else
  {
    printer_attrs = (printer_attrs_t *)erealloc(printer_attrs, 
                                     (num_attrs + 1) * sizeof(printer_attrs_t));

    printer_attrs[num_attrs].name = (char *)emalloc(strlen(name)+1);
    printer_attrs[num_attrs].value = (char *)emalloc(strlen(value)+1);
    strcpy( printer_attrs[num_attrs].name, name );
    strcpy( printer_attrs[num_attrs].value, value );
    num_attrs++;
  }
  return(num_attrs);
}



/*
 *  Local function - get printer attributes.
 */
int _phpcups_get_printer_status(char *name )
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */

  char          printer_uri[1024];
  char          temp[1024];
  static char  *req_attrs[] = {"printer-state", "printer-state-reason" };
  int          i;

  if (name == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect(name, NULL, NULL))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  sprintf(printer_uri, "ipp://localhost/printers/%-s", name );
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, printer_uri );

 /*
  * Do the request and get back a response...
  */

  num_attrs = 0;
  printer_attrs = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {

      if (attr->num_values < 1)
        continue;

      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-state") == 0 &&
          attr->value_tag == IPP_TAG_ENUM)
      {
	strcpy( temp, "unknown" );
        switch( attr->values[0].integer )
        {
          case 3: strcpy( temp, "idle" );
                  break;
          case 4: strcpy( temp, "processing" );
                  break;
          case 5: strcpy( temp, "stopped" );
                  break;
          default: continue;
        }
        _phpcups_update_attrs_list( attr->name, temp );
      }
      else if (attr->name != NULL &&
               (attr->value_tag == IPP_TAG_TEXT ||
                attr->value_tag == IPP_TAG_URI ||
                attr->value_tag == IPP_TAG_STRING))
      {
        for (i=0; i < attr->num_values; i++)
          _phpcups_update_attrs_list(attr->name, attr->values[i].string.text );
      }
      else if (attr->name != NULL &&
               (attr->value_tag == IPP_TAG_ENUM ||
                attr->value_tag == IPP_TAG_BOOLEAN ||
                attr->value_tag == IPP_TAG_INTEGER))
      {
        for (i=0; i < attr->num_values; i++)
        {
          sprintf(temp,"%-d", attr->values[i].integer );
          _phpcups_update_attrs_list(attr->name, temp );
        }
      }
      else if (attr->name != NULL &&
               attr->value_tag == IPP_TAG_RESOLUTION)
      {
        for (i=0; i < attr->num_values; i++)
        {
          sprintf(temp,"X:%-d Y:%-d U:%-d",
                         attr->values[i].resolution.xres,
                         attr->values[i].resolution.yres,
                         attr->values[i].resolution.units );
          _phpcups_update_attrs_list(attr->name, temp );
        }
      }
      else if (attr->name != NULL &&
               attr->value_tag == IPP_TAG_RANGE)
      {
        for (i=0; i < attr->num_values; i++)
        {
          sprintf(temp,"%d-%d",
                         attr->values[i].range.lower,
                         attr->values[i].range.upper );
          _phpcups_update_attrs_list(attr->name, temp );
        }
      }
    }
    ippDelete(response);
  }
  else
  {
    last_error = IPP_BAD_REQUEST;
    return(0);
  }
  return (num_attrs);
}



/*
 *  Functions from the CUPS distribution - util.c
 *
 *
 *
 * Contents:
 *
 *   cupsCancelJob()     - Cancel a print job.
 *   cupsDoFileRequest() - Do an IPP request...
 *   cupsFreeJobs()      - Free memory used by job data.
 *   cupsGetClasses()    - Get a list of printer classes.
 *   cupsGetDefault()    - Get the default printer or class.
 *   cupsGetJobs()       - Get the jobs from the server.
 *   cupsGetPPD()        - Get the PPD file for a printer.
 *   cupsGetPrinters()   - Get a list of printers.
 *   cupsLastError()     - Return the last IPP error that occurred.
 *   cupsPrintFile()     - Print a file to a printer or class.
 *   cupsPrintFiles()    - Print one or more files to a printer or class.
 *   cups_connect()      - Connect to the specified host...
 *   cups_local_auth()   - Get the local authorization certificate if
 *                         available/applicable...
 */



/*
 * 'cupsCancelJob()' - Cancel a print job.
 */

int				/* O - 1 on success, 0 on failure */
cupsCancelJob(const char *name,	/* I - Name of printer or class */
              int        job)	/* I - Job ID */
{
  char		printer[HTTP_MAX_URI],	/* Printer name */
		hostname[HTTP_MAX_URI],	/* Hostname */
		uri[HTTP_MAX_URI];	/* Printer URI */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  cups_lang_t	*language;		/* Language info */


 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build an IPP_CANCEL_JOB request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    job-id
  *    [requesting-user-name]
  */

  request = ippNew();

  request->request.op.operation_id = IPP_CANCEL_JOB;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddInteger(request, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "job-id", job);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

 /*
  * Do the request...
  */

  if ((response = cupsDoRequest(cups_server, request, "/jobs/")) == NULL)
  {
    last_error = IPP_BAD_REQUEST;
    return (0);
  }
  else
  {
    last_error = response->request.status.status_code;
    ippDelete(response);

    return (1);
  }
}


/*
 * 'cupsDoFileRequest()' - Do an IPP request...
 */

ipp_t *					/* O - Response data */
cupsDoFileRequest(http_t     *http,	/* I - HTTP connection to server */
                  ipp_t      *request,	/* I - IPP request */
                  const char *resource,	/* I - HTTP resource for POST */
		  const char *filename)	/* I - File to send or NULL */
{
  ipp_t		*response;		/* IPP response data */
  char		length[255];		/* Content-Length field */
  http_status_t	status;			/* Status of HTTP request */
  FILE		*file;			/* File to send */
  struct stat	fileinfo;		/* File information */
  int		bytes;			/* Number of bytes read/written */
  char		buffer[32768];		/* Output buffer */
  const char	*password;		/* Password string */
  char		realm[HTTP_MAX_VALUE],	/* realm="xyz" string */
		nonce[HTTP_MAX_VALUE],	/* nonce="xyz" string */
		plain[255],		/* Plaintext username:password */
		encode[512];		/* Encoded username:password */
  char		prompt[1024];		/* Prompt string */
  int		digest_tries;		/* Number of tries with Digest */


  if (http == NULL || request == NULL || resource == NULL)
  {
    if (request != NULL)
      ippDelete(request);

    last_error = IPP_INTERNAL_ERROR;
    return (NULL);
  }

  DEBUG_printf(("cupsDoFileRequest(%p, %08x, \'%s\', \'%s\')\n",
                http, request, resource, filename ? filename : "(null)"));

 /*
  * See if we have a file to send...
  */

  if (filename != NULL)
  {
    if (stat(filename, &fileinfo))
    {
     /*
      * Can't get file information!
      */

      ippDelete(request);
      last_error = IPP_NOT_FOUND;
      return (NULL);
    }

    if ((file = fopen(filename, "rb")) == NULL)
    {
     /*
      * Can't open file!
      */

      ippDelete(request);
      last_error = IPP_NOT_FOUND;
      return (NULL);
    }
  }
  else
    file = NULL;

 /*
  * Loop until we can send the request without authorization problems.
  */

  response     = NULL;
  status       = HTTP_ERROR;
  digest_tries = 0;

  while (response == NULL)
  {
    DEBUG_puts("cupsDoFileRequest: setup...");

   /*
    * Setup the HTTP variables needed...
    */

    if (filename != NULL)
      sprintf(length, "%lu", (unsigned long)(ippLength(request) +
                                             (size_t)fileinfo.st_size));
    else
      sprintf(length, "%lu", (unsigned long)ippLength(request));

    httpClearFields(http);
    httpSetField(http, HTTP_FIELD_CONTENT_LENGTH, length);
    httpSetField(http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
    httpSetField(http, HTTP_FIELD_AUTHORIZATION, authstring);

   /*
    * Try the request...
    */

    DEBUG_puts("cupsDoFileRequest: post...");

    if (httpPost(http, resource))
    {
      if (httpReconnect(http))
      {
        status = HTTP_ERROR;
        break;
      }
      else
        continue;
    }

   /*
    * Send the IPP data and wait for the response...
    */

    DEBUG_puts("cupsDoFileRequest: ipp write...");

    request->state = IPP_IDLE;
    if (ippWrite(http, request) != IPP_ERROR)
      if (filename != NULL)
      {
        DEBUG_puts("cupsDoFileRequest: file write...");

       /*
        * Send the file...
        */

        rewind(file);

        while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0)
  	  if (httpWrite(http, buffer, bytes) < bytes)
            break;
      }

   /*
    * Get the server's return status...
    */

    DEBUG_puts("cupsDoFileRequest: update...");

    while ((status = httpUpdate(http)) == HTTP_CONTINUE);

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsDoFileRequest: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * See if we can do local authentication...
      */

      if (cups_local_auth(http))
        continue;

     /*
      * See if we should retry the current digest password...
      */

      if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Basic", 5) == 0 ||
          digest_tries > 1 || !pwdstring[0])
      {
       /*
	* Nope - get a password from the user...
	*/

	snprintf(prompt, sizeof(prompt), "Password for %s on %s? ", cupsUser(),
        	 http->hostname);

        if ((password = cupsGetPassword(prompt)) == NULL)
	  break;
	if (!password[0])
	  break;

        strncpy(pwdstring, password, sizeof(pwdstring) - 1);
	pwdstring[sizeof(pwdstring) - 1] = '\0';

        digest_tries = 0;
      }
      else
        digest_tries ++;

     /*
      * Got a password; encode it for the server...
      */

      if (strncmp(http->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Basic", 5) == 0)
      {
       /*
	* Basic authentication...
	*/

	snprintf(plain, sizeof(plain), "%s:%s", cupsUser(), pwdstring);
	httpEncode64(encode, plain);
	snprintf(authstring, sizeof(authstring), "Basic %s", encode);
      }
      else
      {
       /*
	* Digest authentication...
	*/

        httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "realm", realm);
        httpGetSubField(http, HTTP_FIELD_WWW_AUTHENTICATE, "nonce", nonce);

	httpMD5(cupsUser(), realm, pwdstring, encode);
	httpMD5Final(nonce, "POST", resource, encode);
	snprintf(authstring, sizeof(authstring),
	         "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
	         "response=\"%s\"", cupsUser(), realm, nonce, encode);
      }

      continue;
    }
    else if (status == HTTP_ERROR)
    {
#ifdef WIN32
      if (http->error != WSAENETDOWN && http->error != WSAENETUNREACH)
#else
      if (http->error != ENETDOWN && http->error != ENETUNREACH)
#endif /* WIN32 */
        continue;
      else
        break;
    }
#ifdef HAVE_LIBSSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
     /*
      * Flush any error message...
      */

      httpFlush(http);

     /*
      * Upgrade with encryption...
      */

      httpEncryption(http, HTTP_ENCRYPT_REQUIRED);

     /*
      * Try again, this time with encryption enabled...
      */

      continue;
    }
#endif /* HAVE_LIBSSL */
    else if (status != HTTP_OK)
    {
      DEBUG_printf(("cupsDoFileRequest: error %d...\n", status));

     /*
      * Flush any error message...
      */

      httpFlush(http);
      break;
    }
    else
    {
     /*
      * Read the response...
      */

      DEBUG_puts("cupsDoFileRequest: response...");

      response = ippNew();

      if (ippRead(http, response) == IPP_ERROR)
      {
       /*
        * Delete the response...
	*/

	ippDelete(response);
	response = NULL;

        last_error = IPP_SERVICE_UNAVAILABLE;
	break;
      }
    }
  }

 /*
  * Close the file if needed...
  */

  if (filename != NULL)
    fclose(file);

 /*
  * Flush any remaining data...
  */

  httpFlush(http);

 /*
  * Delete the original request and return the response...
  */
  
  ippDelete(request);

  if (response)
    last_error = response->request.status.status_code;
  else if (status == HTTP_NOT_FOUND)
    last_error = IPP_NOT_FOUND;
  else if (status == HTTP_UNAUTHORIZED)
    last_error = IPP_NOT_AUTHORIZED;
  else if (status != HTTP_OK)
    last_error = IPP_SERVICE_UNAVAILABLE;

  return (response);
}


/*
 * 'cupsFreeJobs()' - Free memory used by job data.
 */

void
cupsFreeJobs(int        num_jobs,/* I - Number of jobs */
             cups_job_t *jobs)	/* I - Jobs */
{
  int	i;			/* Looping var */


  if (num_jobs <= 0 || jobs == NULL)
    return;

  for (i = 0; i < num_jobs; i ++)
  {
    efree(jobs[i].dest);
    efree(jobs[i].user);
    efree(jobs[i].format);
    efree(jobs[i].title);
  }

  efree(jobs);
}


/*
 * 'cupsGetClasses()' - Get a list of printer classes.
 */

int				/* O - Number of classes */
cupsGetClasses(char ***classes)	/* O - Classes */
{
  int		n;		/* Number of classes */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		**temp;		/* Temporary pointer */


  if (classes == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build a CUPS_GET_CLASSES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_CLASSES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

 /*
  * Do the request and get back a response...
  */

  n        = 0;
  *classes = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = emalloc(sizeof(char *));
	else
	  temp = erealloc(*classes, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

          while (n > 0)
	  {
	    n --;
	    efree((*classes)[n]);
	  }

	  efree(*classes);
	  ippDelete(response);
	  return (0);
	}

        *classes = temp;
        temp[n]  = estrdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsGetDefault()' - Get the default printer or class.
 */

const char *			/* O - Default printer or NULL */
cupsGetDefault(void)
{
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  const char	*var;		/* Environment variable */
  static char	def_printer[256];/* Default printer */


 /*
  * First see if the LPDEST or PRINTER environment variables are
  * set...  However, if PRINTER is set to "lp", ignore it to work
  * around a "feature" in most Linux distributions - the default
  * user login scripts set PRINTER to "lp"...
  */

  if ((var = getenv("LPDEST")) != NULL)
    return (var);
  else if ((var = getenv("PRINTER")) != NULL && strcmp(var, "lp") != 0)
    return (var);

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }

 /*
  * Build a CUPS_GET_DEFAULT request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_DEFAULT;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    if ((attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME)) != NULL)
    {
      strncpy(def_printer, attr->values[0].string.text, sizeof(def_printer) - 1);
      def_printer[sizeof(def_printer) - 1] = '\0';
      ippDelete(response);
      return (def_printer);
    }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (NULL);
}


/*
 * 'cupsGetJobs()' - Get the jobs from the server.
 */

int					/* O - Number of jobs */
cupsGetJobs(cups_job_t **jobs,		/* O - Job data */
            const char *mydest,		/* I - Only show jobs for dest? */
            int        myjobs,		/* I - Only show my jobs? */
	    int        completed)	/* I - Only show completed jobs? */
{
  int		n;			/* Number of jobs */
  ipp_t		*request,		/* IPP Request */
		*response;		/* IPP Response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Default language */
  cups_job_t	*temp;			/* Temporary pointer */
  int		id,			/* job-id */
		priority,		/* job-priority */
		size;			/* job-k-octets */
  ipp_jstate_t	state;			/* job-state */
  time_t	completed_time,		/* time-at-completed */
		creation_time,		/* time-at-creation */
		processing_time;	/* time-at-processing */
  const char	*dest,			/* job-printer-uri */
		*format,		/* document-format */
		*title,			/* job-name */
		*user;			/* job-originating-user-name */
  char		uri[HTTP_MAX_URI];	/* URI for jobs */
  static const char *attrs[] =		/* Requested attributes */
		{
		  "job-id",
		  "job-priority",
		  "job-k-octets",
		  "job-state",
		  "time-at-completed",
		  "time-at-creation",
		  "time-at-processing",
		  "job-printer-uri",
		  "document-format",
		  "job-name",
		  "job-originating-user-name"
		};


  if (jobs == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build an IPP_GET_JOBS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requesting-user-name
  *    which-jobs
  *    my-jobs
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_JOBS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  if (mydest)
    snprintf(uri, sizeof(uri), "ipp://localhost/printers/%s", mydest);
  else
    strcpy(uri, "ipp://localhost/jobs");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
               "requesting-user-name", NULL, cupsUser());

  if (myjobs)
    ippAddBoolean(request, IPP_TAG_OPERATION, "my-jobs", 1);

  if (completed)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "which-jobs", NULL, "completed");

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                "requested-attributes", sizeof(attrs) / sizeof(attrs[0]),
		NULL, attrs);

 /*
  * Do the request and get back a response...
  */

  n     = 0;
  *jobs = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
     /*
      * Skip leading attributes until we hit a job...
      */

      while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        attr = attr->next;

      if (attr == NULL)
        break;

     /*
      * Pull the needed attributes from this job...
      */

      id              = 0;
      size            = 0;
      priority        = 50;
      state           = IPP_JOB_PENDING;
      user            = NULL;
      dest            = NULL;
      format          = NULL;
      title           = NULL;
      creation_time   = 0;
      completed_time  = 0;
      processing_time = 0;

      while (attr != NULL && attr->group_tag == IPP_TAG_JOB)
      {
        if (strcmp(attr->name, "job-id") == 0 &&
	    attr->value_tag == IPP_TAG_INTEGER)
	  id = attr->values[0].integer;
        else if (strcmp(attr->name, "job-state") == 0 &&
	         attr->value_tag == IPP_TAG_ENUM)
	  state = (ipp_jstate_t)attr->values[0].integer;
        else if (strcmp(attr->name, "job-priority") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  priority = attr->values[0].integer;
        else if (strcmp(attr->name, "job-k-octets") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  size = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-completed") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  completed_time = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-creation") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  creation_time = attr->values[0].integer;
        else if (strcmp(attr->name, "time-at-processing") == 0 &&
	         attr->value_tag == IPP_TAG_INTEGER)
	  processing_time = attr->values[0].integer;
        else if (strcmp(attr->name, "job-printer-uri") == 0 &&
	         attr->value_tag == IPP_TAG_URI)
	{
	  if ((dest = strrchr(attr->values[0].string.text, '/')) != NULL)
	    dest ++;
        }
        else if (strcmp(attr->name, "job-originating-user-name") == 0 &&
	         attr->value_tag == IPP_TAG_NAME)
	  user = attr->values[0].string.text;
        else if (strcmp(attr->name, "document-format") == 0 &&
	         attr->value_tag == IPP_TAG_MIMETYPE)
	  format = attr->values[0].string.text;
        else if (strcmp(attr->name, "job-name") == 0 &&
	         (attr->value_tag == IPP_TAG_TEXT ||
		  attr->value_tag == IPP_TAG_NAME))
	  title = attr->values[0].string.text;

        attr = attr->next;
      }

     /*
      * See if we have everything needed...
      */

      if (dest == NULL || title == NULL || user == NULL || id == 0)
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      if (format == NULL)
        format = "application/octet-stream";

     /*
      * Allocate memory for the job...
      */

      if (n == 0)
        temp = emalloc(sizeof(cups_job_t));
      else
	temp = erealloc(*jobs, sizeof(cups_job_t) * (n + 1));

      if (temp == NULL)
      {
       /*
        * Ran out of memory!
        */

	cupsFreeJobs(n, *jobs);
	*jobs = NULL;

        ippDelete(response);
	return (0);
      }

      *jobs = temp;
      temp  += n;
      n ++;

     /*
      * Copy the data over...
      */

      temp->dest            = estrdup(dest);
      temp->user            = estrdup(user);
      temp->format          = estrdup(format);
      temp->title           = estrdup(title);
      temp->id              = id;
      temp->priority        = priority;
      temp->state           = state;
      temp->size            = size;
      temp->completed_time  = completed_time;
      temp->creation_time   = creation_time;
      temp->processing_time = processing_time;

      if (attr == NULL)
        break;
    }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsGetPPD()' - Get the PPD file for a printer.
 */

const char *				/* O - Filename for PPD file */
cupsGetPPD(const char *name)		/* I - Printer name */
{
  int		i;			/* Looping var */
  ipp_t		*request,		/* IPP request */
		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* Current attribute */
  cups_lang_t	*language;		/* Local language */
  int		fd;			/* PPD file */
  int		bytes;			/* Number of bytes read */
  char		buffer[8192];		/* Buffer for file */
  char		printer[HTTP_MAX_URI],	/* Printer name */
		method[HTTP_MAX_URI],	/* Method/scheme name */
		username[HTTP_MAX_URI],	/* Username:password */
		hostname[HTTP_MAX_URI],	/* Hostname */
		resource[HTTP_MAX_URI];	/* Resource name */
  int		port;			/* Port number */
  const char	*password;		/* Password string */
  char		realm[HTTP_MAX_VALUE],	/* realm="xyz" string */
		nonce[HTTP_MAX_VALUE],	/* nonce="xyz" string */
		plain[255],		/* Plaintext username:password */
		encode[512];		/* Encoded username:password */
  http_status_t	status;			/* HTTP status from server */
  char		prompt[1024];		/* Prompt string */
  int		digest_tries;		/* Number of tries with Digest */
  static char	filename[HTTP_MAX_URI];	/* Local filename */
  static const char *requested_attrs[] =/* Requested attributes */
		{
		  "printer-uri-supported",
		  "printer-type",
		  "member-uris"
		};


  if (name == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (NULL);
  }

 /*
  * See if we can connect to the server...
  */

  if (!cups_connect(name, printer, hostname))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }

 /*
  * Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    printer-uri
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  snprintf(buffer, sizeof(buffer), "ipp://localhost/printers/%s", printer);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
               "printer-uri", NULL, buffer);

  ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                "requested-attributes",
		sizeof(requested_attrs) / sizeof(requested_attrs[0]),
		NULL, requested_attrs);

 /*
  * Do the request and get back a response...
  */

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error  = response->request.status.status_code;
    printer[0]  = '\0';
    hostname[0] = '\0';

    if ((attr = ippFindAttribute(response, "member-uris", IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the first actual server and printer name in the class...
      */

      for (i = 0; i < attr->num_values; i ++)
      {
	httpSeparate(attr->values[0].string.text, method, username, hostname,
	             &port, resource);
	if (strncmp(resource, "/printers/", 10) == 0)
	{
	 /*
	  * Found a printer!
	  */

	  strncpy(printer, resource + 10, sizeof(printer) - 1);
	  printer[sizeof(printer) - 1] = '\0';
	  break;
	}
      }
    }
    else if ((attr = ippFindAttribute(response, "printer-uri-supported",
                                      IPP_TAG_URI)) != NULL)
    {
     /*
      * Get the actual server and printer names...
      */

      httpSeparate(attr->values[0].string.text, method, username, hostname,
	           &port, resource);
      strncpy(printer, strrchr(resource, '/') + 1, sizeof(printer) - 1);
      printer[sizeof(printer) - 1] = '\0';
    }

    ippDelete(response);

   /*
    * Remap local hostname to localhost...
    */

    gethostname(buffer, sizeof(buffer));

    if (strcasecmp(buffer, hostname) == 0)
      strcpy(hostname, "localhost");
  }

  cupsLangFree(language);

  if (!printer[0])
    return (NULL);

 /*
  * Reconnect to the correct server as needed...
  */

  if (strcasecmp(cups_server->hostname, hostname) != 0)
  {
    httpClose(cups_server);

    if ((cups_server = httpConnectEncrypt(hostname, ippPort(),
                                          cupsEncryption())) == NULL)
    {
      last_error = IPP_SERVICE_UNAVAILABLE;
      return (NULL);
    }
  }

 /*
  * Get a temp file...
  */

  if ((fd = cupsTempFd(filename, sizeof(filename))) < 0)
  {
   /*
    * Can't open file; close the server connection and return NULL...
    */

    httpFlush(cups_server);
    httpClose(cups_server);
    cups_server = NULL;
    return (NULL);
  }

 /*
  * And send a request to the HTTP server...
  */

  snprintf(resource, sizeof(resource), "/printers/%s.ppd", printer);

  digest_tries = 0;

  do
  {
    httpClearFields(cups_server);
    httpSetField(cups_server, HTTP_FIELD_HOST, hostname);
    httpSetField(cups_server, HTTP_FIELD_AUTHORIZATION, authstring);

    if (httpGet(cups_server, resource))
    {
      if (httpReconnect(cups_server))
      {
        status = HTTP_ERROR;
	break;
      }
      else
      {
        status = HTTP_UNAUTHORIZED;
        continue;
      }
    }

    while ((status = httpUpdate(cups_server)) == HTTP_CONTINUE);

    if (status == HTTP_UNAUTHORIZED)
    {
      DEBUG_puts("cupsGetPPD: unauthorized...");

     /*
      * Flush any error message...
      */

      httpFlush(cups_server);

     /*
      * See if we can do local authentication...
      */

      if (cups_local_auth(cups_server))
        continue;

     /*
      * See if we should retry the current digest password...
      */

      if (strncmp(cups_server->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Basic", 5) == 0 ||
          digest_tries > 1 || !pwdstring[0])
      {
       /*
	* Nope - get a password from the user...
	*/

	snprintf(prompt, sizeof(prompt), "Password for %s on %s? ", cupsUser(),
        	 cups_server->hostname);

        if ((password = cupsGetPassword(prompt)) == NULL)
	  break;
	if (!password[0])
	  break;

        strncpy(pwdstring, password, sizeof(pwdstring) - 1);
	pwdstring[sizeof(pwdstring) - 1] = '\0';

        digest_tries = 0;
      }
      else
        digest_tries ++;

     /*
      * Got a password; encode it for the server...
      */

      if (strncmp(cups_server->fields[HTTP_FIELD_WWW_AUTHENTICATE], "Basic", 5) == 0)
      {
       /*
	* Basic authentication...
	*/

	snprintf(plain, sizeof(plain), "%s:%s", cupsUser(), pwdstring);
	httpEncode64(encode, plain);
	snprintf(authstring, sizeof(authstring), "Basic %s", encode);
      }
      else
      {
       /*
	* Digest authentication...
	*/

        httpGetSubField(cups_server, HTTP_FIELD_WWW_AUTHENTICATE, "realm", realm);
        httpGetSubField(cups_server, HTTP_FIELD_WWW_AUTHENTICATE, "nonce", nonce);

	httpMD5(cupsUser(), realm, pwdstring, encode);
	httpMD5Final(nonce, "GET", resource, encode);
	snprintf(authstring, sizeof(authstring),
	         "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
	         "response=\"%s\"", cupsUser(), realm, nonce, encode);
      }

      continue;
    }
#ifdef HAVE_LIBSSL
    else if (status == HTTP_UPGRADE_REQUIRED)
    {
     /*
      * Flush any error message...
      */

      httpFlush(cups_server);

     /*
      * Upgrade with encryption...
      */

      httpEncryption(cups_server, HTTP_ENCRYPT_REQUIRED);

     /*
      * Try again, this time with encryption enabled...
      */

      continue;
    }
#endif /* HAVE_LIBSSL */
  }
  while (status == HTTP_UNAUTHORIZED || status == HTTP_UPGRADE_REQUIRED);

 /*
  * See if we actually got the file or an error...
  */

  if (status != HTTP_OK)
  {
    unlink(filename);
    httpFlush(cups_server);
    httpClose(cups_server);
    cups_server = NULL;
    return (NULL);
  }

 /*
  * OK, we need to copy the file...
  */

  while ((bytes = httpRead(cups_server, buffer, sizeof(buffer))) > 0)
    write(fd, buffer, bytes);

  close(fd);

  return (filename);
}


/*
 * 'cupsGetPrinters()' - Get a list of printers.
 */

int					/* O - Number of printers */
cupsGetPrinters(char ***printers)	/* O - Printers */
{
  int		n;		/* Number of printers */
  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */
  char		**temp;		/* Temporary pointer */


  if (printers == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if (!cups_connect("default", NULL, NULL))
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

 /*
  * Build a CUPS_GET_PRINTERS request, which requires the following
  * attributes:
  *
  *    attributes-charset
  *    attributes-natural-language
  *    requested-attributes
  */

  request = ippNew();

  request->request.op.operation_id = CUPS_GET_PRINTERS;
  request->request.op.request_id   = 1;

  language = cupsLangDefault();

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL, language->language);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
               "requested-attributes", NULL, "printer-name");

 /*
  * Do the request and get back a response...
  */

  n         = 0;
  *printers = NULL;

  if ((response = cupsDoRequest(cups_server, request, "/")) != NULL)
  {
    last_error = response->request.status.status_code;

    for (attr = response->attrs; attr != NULL; attr = attr->next)
      if (attr->name != NULL &&
          strcasecmp(attr->name, "printer-name") == 0 &&
          attr->value_tag == IPP_TAG_NAME)
      {
        if (n == 0)
	  temp = emalloc(sizeof(char *));
	else
	  temp = erealloc(*printers, sizeof(char *) * (n + 1));

	if (temp == NULL)
	{
	 /*
	  * Ran out of memory!
	  */

	  while (n > 0)
	  {
	    n --;
	    efree((*printers)[n]);
	  }

	  efree(*printers);
	  ippDelete(response);
	  return (0);
	}

        *printers = temp;
        temp[n]   = estrdup(attr->values[0].string.text);
	n ++;
      }

    ippDelete(response);
  }
  else
    last_error = IPP_BAD_REQUEST;

  return (n);
}


/*
 * 'cupsLastError()' - Return the last IPP error that occurred.
 */

ipp_status_t		/* O - IPP error code */
cupsLastError(void)
{
  return (last_error);
}


/*
 * 'cupsPrintFile()' - Print a file to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFile(const char    *name,	/* I - Printer or class name */
              const char    *filename,	/* I - File to print */
	      const char    *title,	/* I - Title of job */
              int           num_options,/* I - Number of options */
	      cups_option_t *options)	/* I - Options */
{
  DEBUG_printf(("cupsPrintFile(\'%s\', \'%s\', %d, %p)\n",
                name, filename, num_options, options));

  return (cupsPrintFiles(name, 1, &filename, title, num_options, options));
}


/*
 * 'cupsPrintFiles()' - Print one or more files to a printer or class.
 */

int					/* O - Job ID */
cupsPrintFiles(const char    *name,	/* I - Printer or class name */
               int           num_files,	/* I - Number of files */
               const char    **files,	/* I - File(s) to print */
	       const char    *title,	/* I - Title of job */
               int           num_options,/* I - Number of options */
	       cups_option_t *options)	/* I - Options */
{
  int		i;			/* Looping var */
  const char	*val;			/* Pointer to option value */
  ipp_t		*request;		/* IPP request */
  ipp_t		*response;		/* IPP response */
  ipp_attribute_t *attr;		/* IPP job-id attribute */
  char		hostname[HTTP_MAX_URI],	/* Hostname */
		printer[HTTP_MAX_URI],	/* Printer or class name */
		uri[HTTP_MAX_URI];	/* Printer URI */
  cups_lang_t	*language;		/* Language to use */
  int		jobid;			/* New job ID */


  DEBUG_printf(("cupsPrintFiles(\'%s\', %d, %p, %d, %p)\n",
                name, num_files, files, num_options, options));

  if (name == NULL || num_files < 1 || files == NULL)
    return (0);

 /*
  * Setup a connection and request data...
  */

  if (!cups_connect(name, printer, hostname))
  {
    DEBUG_printf(("cupsPrintFile: Unable to open connection - %s.\n",
                  strerror(errno)));
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (0);
  }

  language = cupsLangDefault();

 /*
  * Build a standard CUPS URI for the printer and fill the standard IPP
  * attributes...
  */

  if ((request = ippNew()) == NULL)
    return (0);

  request->request.op.operation_id = num_files == 1 ? IPP_PRINT_JOB :
                                                      IPP_CREATE_JOB;
  request->request.op.request_id   = 1;

  snprintf(uri, sizeof(uri), "ipp://%s:%d/printers/%s", hostname, ippPort(), printer);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
               "attributes-charset", NULL, cupsLangEncoding(language));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
               "attributes-natural-language", NULL,
               language != NULL ? language->language : "C");

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
               NULL, uri);

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
               NULL, cupsUser());

  if (title)
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL, title);

 /*
  * Then add all options...
  */

  cupsEncodeOptions(request, num_options, options);

 /*
  * Do the request...
  */

  snprintf(uri, sizeof(uri), "/printers/%s", printer);

  if (num_files == 1)
    response = cupsDoFileRequest(cups_server, request, uri, *files);
  else
    response = cupsDoRequest(cups_server, request, uri);

  if (response == NULL)
    jobid = 0;
  else if (response->request.status.status_code > IPP_OK_CONFLICT)
  {
    DEBUG_printf(("IPP response code was 0x%x!\n",
                  response->request.status.status_code));
    jobid = 0;
  }
  else if ((attr = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER)) == NULL)
  {
    DEBUG_puts("No job ID!");
    jobid = 0;
  }
  else
    jobid = attr->values[0].integer;

  if (response != NULL)
    ippDelete(response);

 /*
  * Handle multiple file jobs if the create-job operation worked...
  */

  if (jobid > 0 && num_files > 1)
    for (i = 0; i < num_files; i ++)
    {
     /*
      * Build a standard CUPS URI for the job and fill the standard IPP
      * attributes...
      */

      if ((request = ippNew()) == NULL)
	return (0);

      request->request.op.operation_id = IPP_SEND_DOCUMENT;
      request->request.op.request_id   = 1;

      snprintf(uri, sizeof(uri), "ipp://%s:%d/jobs/%d", hostname, ippPort(),
               jobid);

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
        	   "attributes-charset", NULL, cupsLangEncoding(language));

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	   "attributes-natural-language", NULL,
        	   language != NULL ? language->language : "C");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri",
        	   NULL, uri);

     /*
      * Handle raw print files...
      */

      if (cupsGetOption("raw", num_options, options))
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, "application/vnd.cups-raw");
      else if ((val = cupsGetOption("document-format", num_options, options)) != NULL)
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, val);
      else
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_MIMETYPE, "document-format",
        	     NULL, "application/octet-stream");

      ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	   NULL, cupsUser());

     /*
      * Is this the last document?
      */

      if (i == (num_files - 1))
        ippAddBoolean(request, IPP_TAG_OPERATION, "last-document", 1);

     /*
      * Send the file...
      */

      snprintf(uri, sizeof(uri), "/printers/%s", printer);

      if ((response = cupsDoFileRequest(cups_server, request, uri,
                                        files[i])) != NULL)
	ippDelete(response);
    }

  return (jobid);
}


/*
 * 'cups_connect()' - Connect to the specified host...
 */

static char *				/* I - Printer name or NULL */
cups_connect(const char *name,		/* I - Destination (printer[@host]) */
	     char       *printer,	/* O - Printer name [HTTP_MAX_URI] */
             char       *hostname)	/* O - Hostname [HTTP_MAX_URI] */
{
  char		hostbuf[HTTP_MAX_URI];	/* Name of host */
  static char	printerbuf[HTTP_MAX_URI];
					/* Name of printer or class */


  DEBUG_printf(("cups_connect(\"%s\", %p, %p)\n", name, printer, hostname));

  if (name == NULL)
  {
    last_error = IPP_BAD_REQUEST;
    return (NULL);
  }

  if (sscanf(name, "%1023[^@]@%1023s", printerbuf, hostbuf) == 1)
  {
    strncpy(hostbuf, cupsServer(), sizeof(hostbuf) - 1);
    hostbuf[sizeof(hostbuf) - 1] = '\0';
  }

  if (hostname != NULL)
  {
    strncpy(hostname, hostbuf, HTTP_MAX_URI - 1);
    hostname[HTTP_MAX_URI - 1] = '\0';
  }
  else
    hostname = hostbuf;

  if (printer != NULL)
  {
    strncpy(printer, printerbuf, HTTP_MAX_URI - 1);
    printer[HTTP_MAX_URI - 1] = '\0';
  }
  else
    printer = printerbuf;

  if (cups_server != NULL)
  {
    if (strcasecmp(cups_server->hostname, hostname) == 0)
      return (printer);

    httpClose(cups_server);
  }

  DEBUG_printf(("connecting to %s on port %d...\n", hostname, ippPort()));

  if ((cups_server = httpConnectEncrypt(hostname, ippPort(),
                                        cupsEncryption())) == NULL)
  {
    last_error = IPP_SERVICE_UNAVAILABLE;
    return (NULL);
  }
  else
    return (printer);
}


/*
 * 'cups_local_auth()' - Get the local authorization certificate if
 *                       available/applicable...
 */

static int			/* O - 1 if available, 0 if not */
cups_local_auth(http_t *http)	/* I - Connection */
{
#if defined(WIN32) || defined(__EMX__)
 /*
  * Currently WIN32 and OS-2 do not support the CUPS server...
  */

  return (0);
#else
  int		pid;		/* Current process ID */
  FILE		*fp;		/* Certificate file */
  char		filename[1024],	/* Certificate filename */
		certificate[33];/* Certificate string */
  const char	*root;		/* Server root directory */


 /*
  * See if we are accessing localhost...
  */

  if (ntohl(http->hostaddr.sin_addr.s_addr) != 0x7f000001 &&
      strcasecmp(http->hostname, "localhost") != 0)
    return (0);

 /*
  * Try opening a certificate file for this PID.  If that fails,
  * try the root certificate...
  */

  if ((root = getenv("CUPS_SERVERROOT")) == NULL)
    root = CUPS_SERVERROOT;

  pid = getpid();
  snprintf(filename, sizeof(filename), "%s/certs/%d", root, pid);
  if ((fp = fopen(filename, "r")) == NULL && pid > 0)
  {
    snprintf(filename, sizeof(filename), "%s/certs/0", root);
    fp = fopen(filename, "r");
  }

  if (fp == NULL)
    return (0);

 /*
  * Read the certificate from the file...
  */

  fgets(certificate, sizeof(certificate), fp);
  fclose(fp);

 /*
  * Set the authorization string and return...
  */

  snprintf(authstring, sizeof(authstring), "Local %s", certificate);

  return (1);
#endif /* WIN32 || __EMX__ */
}


/*
 * End of "$Id: phpcups.c,v 1.1.2.2 2002/11/12 21:17:27 ted Exp $".
 */


/*
 */
