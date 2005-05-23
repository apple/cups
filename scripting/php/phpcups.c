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
 * "$Id$"
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

#include "config.h"
#include "cups.h"
#include "ipp.h"
#include "language.h"
#include "string.h"
#include "debug.h"
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
 *  Date:        8 April 2002 - TDB
 *
 *  Parameters:  d_name     - String  - Name of destination.
 *               d_instance - String  - Name of instance on destination.
 *
 *  Returns:     Array of option "objects", with each object
 *               containing the members:
 *
 *                 name  - String - Option name
 *                 value - String - Option value
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
 *  Date:        8 April 2002 - TDB
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
 *  Date:        8 April 2002 - TDB
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
 *  Date:        8 April 2002 - TDB
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
 *  Date:        8 April 2002 - TDB
 *
 *  Parameters:  server - String  -  Name or IP of cups server.
 *               name   - String  -  Name of destination.
 *               job    - Long    -  Job ID to cancel.
 *
 *  Returns:     error number - Long?????????
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
 *  Date:        8 April 2002 - TDB
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

    bzero( p_server, 256 );
    bzero( p_printer, 256 );
    bzero( p_filename, 256 );
    bzero( p_title, 256 );
    bzero( temp, 4096);
    current = 0;

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

              _zz_internal_log( "cups_print_file", p_server );
              _zz_internal_log( "cups_print_file", p_printer );
              _zz_internal_log( "cups_print_file", p_filename );
              _zz_internal_log( "cups_print_file", p_title );
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

              _zz_internal_log( "cups_print_file(server)", p_server );
              _zz_internal_log( "cups_print_file(printer)", p_printer );
              _zz_internal_log( "cups_print_file(filename)", p_filename );
              _zz_internal_log( "cups_print_file(title)", p_title );

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
              _zz_internal_log( "cups_print_file(option)", temp );
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
      
      bzero(temp,4096);
      sprintf(temp,"(2) - P: %s F: %s T: %s C: %d", p_printer, p_filename,
                                              p_title, current );
      _zz_internal_log( "cups_print_file", temp );
      ret_val = cupsPrintFile( p_printer,p_filename,p_title,current,options);
      for (current=0; current < count; current++)
      {
        efree( options[current].name );
        efree( options[current].value );
      }
      efree(options);
    }
    else
    {
      _zz_internal_log( "cups_print_file", "going to print");
      ret_val = cupsPrintFile( p_printer,p_filename,p_title,0,NULL );
    }

    RETURN_LONG(ret_val);
}


int _phpcups_get_printer_status(char *server, int port, char *name );

typedef struct printer_attrs_type
{
  char      *name;
  char      *value;
} printer_attrs_t;



int              num_attrs = 0;
printer_attrs_t  *printer_attrs = NULL;

void free_attrs_list(void);
void _phpcups_free_attrs_list(void);

/*
 *  Function:    cups_get_printer_attributes
 *
 *  Date:        8 April 2002 - TDB
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
    zval    **z_server, **z_port, **z_name;

    zval    *new_object;

    char    p_server[256], p_name[256];
    int     p_port = 631;

    int     i, count;


    /*
     *  Initialize return value.
     */
    array_init(return_value);

    /*
     *  Get parameters .....
     */
    switch(ZEND_NUM_ARGS())
    {
      //
      //  Destination name only, assume localhost
      //
      case 1: 
              if (zend_get_parameters_ex( 1, &z_name ) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              strcpy( p_server,"localhost" );
              convert_to_string_ex( z_name );
              if ( (char *)(*z_name)->value.str.val != NULL )
                strcpy( p_name,(char *)(*z_name)->value.str.val );
              break;

      //
      //  Server and estination name only, assume port 631
      //
      case 2: 
              if (zend_get_parameters_ex( 2, &z_server, &z_name ) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex( z_server);
              if ((char *)(*z_server)->value.str.val != NULL)
                strcpy( p_server,(char *)(*z_server)->value.str.val );
              convert_to_string_ex( z_name );
              if ( (char *)(*z_name)->value.str.val != NULL )
                strcpy( p_name,(char *)(*z_name)->value.str.val );
              break;

      //
      //  Server, destination name and port.
      //
      case 3: 
              if(zend_get_parameters_ex(3,&z_server,&z_port,&z_name) != SUCCESS)
              {
                WRONG_PARAM_COUNT;
              }
              convert_to_string_ex(z_server);
              if ((char *)(*z_server)->value.str.val != NULL)
                strcpy( p_server,(char *)(*z_server)->value.str.val );
              convert_to_string_ex(z_name);
              if ( (char *)(*z_name)->value.str.val != NULL )
                strcpy( p_name,(char *)(*z_name)->value.str.val );
              convert_to_string_ex(z_port);
              if ( (char *)(*z_port)->value.str.val != NULL )
                p_port = atoi( (char *)(*z_name)->value.str.val );
              break;

      default: WRONG_PARAM_COUNT;
    }

    printer_attrs = NULL;
    count = _phpcups_get_printer_status( p_server, p_port, p_name );

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



int _phpcups_get_printer_status( char *server, int port, char *name )
{
  http_encryption_t encrypt = HTTP_ENCRYPT_IF_REQUESTED;

  ipp_t		*request,	/* IPP Request */
		*response;	/* IPP Response */
  ipp_attribute_t *attr;	/* Current attribute */
  cups_lang_t	*language;	/* Default language */

  char          printer_uri[1024];
  char          temp[1024];
  static char  *req_attrs[] = {"printer-state", "printer-state-reason" };
  int          i;
  FILE         *fp;

  if (name == NULL)
  {
    last_error = IPP_INTERNAL_ERROR;
    return (0);
  }

 /*
  * Try to connect to the server...
  */

  if ((cups_server = httpConnectEncrypt(server, port, encrypt)) == NULL)
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
                attr->value_tag == IPP_TAG_KEYWORD ||
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
          sprintf(temp,"%-dx%-dx%-d",
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


void _zz_internal_log( char *func, char *line )
{
  FILE *fp;

  if ((fp = fopen("/var/log/cups/project.log","a")) == NULL)
	return;

  fprintf(fp,"phpcups: %s - %s\n", func, line );
  fflush(fp);
  fclose(fp);
}



/*
 * End of "$Id$".
 */


/*
 */
