/*
 * "$Id: phpcups.c,v 1.1.2.1 2002/08/19 19:42:18 mike Exp $"
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
 *   cupsGetDests()        - Get a list of destinations and thier
 *                           properites.
 *   cupsGetDestOptions()  - Get a list of options for a specific
 *                           destination.
 *   cupsGetJobs()         - Get a list of jobs for a destination.
 *   cupsCancelJob()       - Cancel a print job.
 *   cupsLastError()       - Get the last CUPS error.
 *   cupsPrintFile()       - Print a file.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phpcups.h"

#include <cups.h>


static int le_result, le_link, le_plink;

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
	PHP_FE(phpcupsGetDests,	NULL)		
	PHP_FE(phpcupsGetDestOptions,	NULL)		
	PHP_FE(phpcupsGetJobs,	NULL)		
	PHP_FE(phpcupsCancelJob,	NULL)		
	PHP_FE(phpcupsLastError,	NULL)		
	PHP_FE(phpcupsPrintFile,	NULL)		
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
 * 
 *  Get the options for a specified printer / instance.
 *
 *  phpcupsGetDestOptions( z_printer  :String: I - Printer name.
 *                         z_instance :String: I - Instance name.
 *                  )
 *
 *  Returns: On success, an array of cups_option_t objects.
 *
 *           Objects in the array contain:
 *
 *             $obj->name    -  String name of an option.
 *             $obj->value   -  String value of an option.
 *
 */
PHP_FUNCTION(phpcupsGetDestOptions)
{
    zval        **z_printer,         /*  Printer name                   */
                **z_instance;        /*  Instance name                  */
    zval        *new_object;         /*  Used to create new objects     */
    char	*p_printer  = NULL,  /* Local strings for convenience.  */
                *p_instance = NULL;  /* ....                            */
    cups_dest_t *dests,              /* List of destinations.           */
                *dptr;               /* Destination pointer.            */
    int         num_dests;           /* Number of destinations found.   */
    int         i, j;                /*  Just for looping.              */

    /*
     *  Setup the return_value to be an array.
     */
    array_init(return_value);

    /*
     *  Get the function parameters from PHP.
     */
    if ((ZEND_NUM_ARGS() != 2) ||
        (zend_get_parameters_ex( 2, &z_printer, &z_instance ) != SUCCESS))
    {
      WRONG_PARAM_COUNT;
    }

    /*
     *  Convert the zval's to local strings.
     */
    convert_to_string_ex( z_printer );
    convert_to_string_ex( z_instance );
    if ( (char *)(*z_printer)->value.str.val != NULL )
      p_printer = estrdup((char *)(*z_printer)->value.str.val );
    if ( (char *)(*z_instance)->value.str.val != NULL )
      p_instance = estrdup((char *)(*z_instance)->value.str.val );

    /*
     *  Find the dest/instance we want options for.
     */
    num_dests = cupsGetDests(&dests); 
    for (i=0, j = -1; (i < num_dests) && (j < 0); i++)
    {
      dptr = &dests[i];

      if (dptr->name == NULL)
	dptr->name = estrdup(" ");
      if (dptr->instance == NULL)
	dptr->instance = estrdup(" ");

      /*
       *  If both a printer name and instance name were provided,
       *  use them to filter the list.
       */
      if (strlen(p_printer) && strlen(p_instance))
      {
        if ((!strcmp( dptr->name, p_printer)) &&
            (!strcmp( dptr->instance, p_instance )))
        {
          /*
           *  Loop through the options, and create an object
           *  for each one.  Add each object to the return_value 
           *  array.
           */
          for (j=0; j < dptr->num_options; j++ )
          {
            MAKE_STD_ZVAL(new_object);
            if (object_init(new_object) == SUCCESS)
            {
              add_property_string(new_object,"name",dptr->options[j].name,1);
              add_property_string(new_object,"value",dptr->options[j].value,1);
              add_next_index_zval( return_value, new_object );
            }
          }
        }
      }
      else if (strlen(p_printer))
      {
        /*
         *  Only a printer name was provided, use that
         *  to filter the list.
         */
        if (!strcmp( dptr->name, p_printer)) 
        {
          /*
           *  Loop through the options, and create an object
           *  for each one.  Add each object to the return_value 
           *  array.
           */
          for (j=0; j < dptr->num_options; j++ )
          {
            MAKE_STD_ZVAL(new_object);
            if (object_init(new_object) == SUCCESS)
            {
              add_property_string(new_object,"name",dptr->options[j].name,1);
              add_property_string(new_object,"value",dptr->options[j].value,1);
              add_next_index_zval( return_value, new_object );
            }
          }
        }
      }
    }

    /*
     *  Free any memory allocated locally.
     */
    cupsFreeDests(num_dests,dests); 
    if (p_printer != NULL)
      efree(p_printer);
    if (p_instance != NULL)
      efree(p_instance);
}




/* 
 *  Get a list of destinations.
 *
 *  phpcupsGetDests( void )
 *
 *  Returns: On success, an array of cups_dest_t objects.
 *
 *           Objects in the array contain:
 *
 *           $obj->name         - Name of printer (string)
 *           $obj->instance     - Name of instance (string)
 *           $obj->is_default   - 1 if true, 0 if false (integer)
 *           $obj->num_options  - Number of options for destination (integer)
 */
PHP_FUNCTION(phpcupsGetDests)
{
    zval        *new_object;     /* Used to create new destination object */
    cups_dest_t *dests,          /* Pointer to array of destinations.     */
                *dptr;           /* Destination pointer.                  */
    int         num_dests;       /* Number of destinations found.         */
    int         i;               /* For looping.                          */

    /*
     *  Initialize the return value as an array.
     */
    array_init(return_value);

    /*
     *  Get the destination list and loop through it.
     */
    num_dests = cupsGetDests(&dests); 
    for (i=0; i < num_dests; i++)
    {
      dptr = &dests[i];

      /*
       *  Create an object for the current destination, and add it to
       *  the return_value array.
       */
      MAKE_STD_ZVAL(new_object);
      if (object_init(new_object) == SUCCESS)
      {
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
    cupsFreeDests(num_dests,dests); 
}


/*
 *
 *  Get a list of jobs for a specified printer.
 *
 *  phpcupsPrintFile( z_printer    :String:  I - Destination name.
 *                    z_myjobs     :Boolean: I - Get only my jobs?
 *                    z_completed  :Boolean: I - Get completed jobs?
 *                  )
 *
 *  Returns: On success, array of cups_job_t objects.
 *
 *           Objects in the array contain:
 *
 *           $obj->id               - Job id (integer)
 *           $obj->dest             - Destination name (string)
 *           $obj->title            - Job title (string)
 *           $obj->user             - Job owner (string)
 *           $obj->format           - Job format (string)
 *           $obj->state            - Job state (integer)
 *           $obj->size             - Job size (integer)
 *           $obj->priority         - Job priority (integer)
 *           $obj->completed_time   - Unix date of completion time (integer)
 *           $obj->creation_time    - Unix date of creation time (integer)
 *           $obj->processing_time  - Number of seconds to process (integer)
 *
 */
PHP_FUNCTION( phpcupsGetJobs )
{
    zval        *new_object;    /* Used to create new job object.        */
    zval        **z_printer,    /* Name of destination printer.          */
                **z_myjobs,     /* Show only my jobs (0/1,false/true)    */
                **z_completed;  /* Show completed jobs (0/1,false/true)  */
    char	*p_printer;     /* Local variables for convenience       */
    int         p_myjobs,       /* ....                                  */
                p_completed;    /* ....                                  */
    cups_job_t  *jobs,          /* Pointer to job list array.            */
                *jptr;          /* Pointer to a job.                     */
    int         num_jobs;       /* Number of jobs in job list.           */
    int         i;              /* For looping.                          */


    p_printer   = NULL;
    p_myjobs    = 0;
    p_completed = 1;

    /*
     *  Initialize the return_value as an array.
     */
    array_init(return_value);

    /*
     *  We don't always need all 3 params, so handle differently.
     */
    switch(ZEND_NUM_ARGS())
    {
      /*
       *  destination only provided.
       */
      case 1: 
              /*
               *  Convert function parameters to local variables.
               */
              if (zend_get_parameters_ex( 1, &z_printer ) == SUCCESS)
              {
                convert_to_string_ex( z_printer );
                if ( (char *)(*z_printer)->value.str.val != NULL )
                  p_printer = estrdup((char *)(*z_printer)->value.str.val );
              }
              break;

      /*
       *  destination and myjobs
       */
      case 2: 
              /*
               *  Convert function parameters to local variables.
               */
              if (zend_get_parameters_ex(2,&z_printer,&z_myjobs) == SUCCESS)
              {
                convert_to_string_ex( z_printer );
                if ( (char *)(*z_printer)->value.str.val != NULL )
                  p_printer = estrdup((char *)(*z_printer)->value.str.val );
                convert_to_string_ex( z_myjobs );
                p_myjobs = strtoul((char *)(*z_myjobs)->value.str.val,NULL,10);
              }
              break;

      /*
       *  destination, myjobs, and completed
       */
      case 3: 
              /*
               *  Convert function parameters to local variables.
               */
              if (zend_get_parameters_ex(3,&z_printer,&z_myjobs,&z_completed) == SUCCESS)
              {
                convert_to_string_ex( z_printer );
                if ( (char *)(*z_printer)->value.str.val != NULL )
                  p_printer = estrdup((char *)(*z_printer)->value.str.val );
                convert_to_string_ex( z_myjobs );
                p_myjobs = strtoul((char *)(*z_myjobs)->value.str.val,NULL,10);
                convert_to_string_ex( z_completed);
                p_completed = strtoul((char *)(*z_completed)->value.str.val,NULL,10);
              }
              break;
    }

    cupsSetUser("root");

    /*
     *  Get the jobs list, and convert to an array of cups_job_t
     *  objects.
     */
    num_jobs = cupsGetJobs(&jobs,p_printer,p_myjobs,p_completed); 
    for (i=0; i < num_jobs; i++)
    {
      jptr = &jobs[i];

      /*
       *  Convert the current job to an object, and add to the
       *  return_value array.
       */
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
    }

    /*
     *  Free any memory allocated locally.
     */
    cupsFreeJobs(num_jobs,jobs); 
    if (p_printer != NULL)
      efree(p_printer);
}




/*
 *  Get the last CUPS error and return it to PHP.
 *
 *  phpcupsLastError(void)
 *
 *  Returns: cupsLastError() result.
 */
PHP_FUNCTION(phpcupsLastError)
{
  RETURN_LONG(cupsLastError());
}



/* 
 *  Cancel a print job on a specified printer.
 *
 *  phpcupsCancelJob( z_printer    :String:  I - Destination name.
 *                    z_job        :Integer: I - Job id to cancel.
 *                  )
 *
 *  Returns: cupsCancelJob() result.
 */
PHP_FUNCTION(phpcupsCancelJob)
{
    zval        **z_printer,         /*  Name of printer to cancel job on. */
                **z_job;             /*  Job id to cancel.                 */
    char	*p_printer = NULL;   /*  Local storage for convenience.    */
    int         p_job      = -1;     /*  ....                              */
    int         ret_val = -1;        /*  Return value.                     */

    /*
     *  Get the function parameters from PHP.
     */
    if ((ZEND_NUM_ARGS() != 2) ||
        (zend_get_parameters_ex( 2, &z_printer, &z_job ) != SUCCESS))
    {
      WRONG_PARAM_COUNT;
    }


    /*
     *  Convert the zval's to local variables.
     */
    convert_to_string_ex( z_printer );
    if ( (char *)(*z_printer)->value.str.val != NULL )
      p_printer = estrdup((char *)(*z_printer)->value.str.val );
    convert_to_string_ex( z_job );
    p_job = strtoul((char *)(*z_job)->value.str.val,NULL,10);

    /*
     *  Cancel the job.
     */
    ret_val = cupsCancelJob(p_printer,p_job);

    /*
     *  Free any memory allocated locally.
     */
    if (p_printer != NULL)
      efree(p_printer); 

    RETURN_LONG(ret_val);
}





/* 
 *
 *  Print a file to the specified printer.
 *
 *  phpcupsPrintFile( z_printer    :String: I - Destination name.
 *                    z_filename   :String: I - Full path to file to print.
 *                    z_title      :String: I - Title of job.
 *                    z_options    :Array:  I - Options for job.
 *                  )
 *
 *  Returns: On success, the job id.  Failure returns -1.
 *
 *
 *  Notes:   The z_options array is a keyed array.  For example, to
 *           create the array from your PHP program:
 *
 *  $options = Array();
 *  $options['jobs-sheets'] = "none,none";
 *
 */
PHP_FUNCTION(phpcupsPrintFile)
{
    zval           **z_printer,    /*  Name of the destination printer.  */
                   **z_filename,   /*  Full path of file to print.       */
                   **z_title;      /*  Title of job.                     */
    pval           **p_options;    /*  Keyed array of options.           */
    HashTable      *p_ht;          /*  Hashtable used to parse options
                                       array.                            */
    zval           **z_data;       /*  Used to get value of hash table
                                       data element.                     */
    char           ht_data[1024],  /*  String value of hashtable data    
                                       element.                          */
                   ht_key[1024];   /*  String value of hashtable key 
                                       element.                          */
    ulong          num_key;        /*  Used to get numeric key value from
                                       hashtable.                        */
    char           *string_key;    /*  Used to get string key value from
                                       hashtable.                        */
    char	   *p_printer  = NULL; /*  Local strings for convenience.  */
    char	   *p_filename = NULL; /*  ....                            */
    char	   *p_title    = NULL; /*  ....                            */
    int            num_options = 0;  /* number of options from array.    */
    cups_option_t  *options = NULL,  /* Options array.                   */
                   *optr = NULL;     /* Temporary option pointer.        */
    int            i,                /* Array index.                     */
                   ret_val = -1;     /* Return value.                    */

/*
    if (ZEND_NUM_ARGS() != 4) 
    {
      printf("\nWrong param count.\n");
      fflush(stdout);
      WRONG_PARAM_COUNT;
    }
*/

    /*
     *  Get the function parameters from PHP.
     */
    if (zend_get_parameters_ex( 4, &z_printer,&z_filename,&z_title,&p_options) != SUCCESS)
    {
      WRONG_PARAM_COUNT;
    }

    /*
     *  Convert the zval's for printer, filename and title to local
     *  strings.
     */
    convert_to_string_ex( z_printer);
    if ( (char *)(*z_printer)->value.str.val != NULL )
      p_printer = estrdup((char *)(*z_printer)->value.str.val );

    convert_to_string_ex( z_filename );
    if ( (char *)(*z_filename)->value.str.val != NULL )
      p_filename = estrdup((char *)(*z_filename)->value.str.val );

    convert_to_string_ex( z_title );
    if ( (char *)(*z_title)->value.str.val != NULL )
      p_title = estrdup((char *)(*z_title)->value.str.val );


    /*
     *  Convert the options array to a hash table.
     */
    p_ht = HASH_OF(*p_options);
    if (p_ht)
    {

      /*
       *  Step through the hash table and get the key/data pairs to
       *  build the cups options array to pass to cupsPrintFile().
       */
      zend_hash_internal_pointer_reset(p_ht);
      while (zend_hash_get_current_data(p_ht,(void **)&z_data ) == SUCCESS)
      {
        /*
         *  Copy the key value to a local string.
         */
        bzero(ht_key,1024);
        switch( zend_hash_get_current_key( p_ht, &string_key, &num_key, 1 ))
        {
          case HASH_KEY_IS_STRING:
                     strcpy( ht_key, string_key );
                     break;

          case HASH_KEY_IS_LONG:
                     sprintf(ht_key,"%d", num_key );
                     break;

          default:   continue;
        }
        /*
         *  Convert the data value to a local string.
         */
        bzero(ht_data,1024);
        convert_to_string_ex( z_data );
        if ( (char *)(*z_data)->value.str.val != NULL )
          strcpy( ht_data,(char *)(*z_data)->value.str.val );

        /*
         *  Allocate memory for the new option.
         */
        if (!num_options)
        {
          options = (cups_option_t *)emalloc(sizeof(cups_option_t));
        }
        else
        {
          options = (cups_option_t *)erealloc(options,(sizeof(cups_option_t) * (num_options + 1)));
        }
 

        /*
         *  And copy the new option into the array.
         */
        if (options != NULL)
        {
          optr = options;
          optr += num_options;
          optr->name  = estrdup( ht_key );
          optr->value = estrdup( ht_data );
          num_options++;
        }

        zend_hash_move_forward(p_ht);

      }  /* end of while hash data .... */

    }
    else
    {
      RETURN_LONG(-1);
    }

    /*
     *  Print the file.
     */
    ret_val = cupsPrintFile( p_printer,p_filename,p_title,num_options,options);

    /*
     *  And free the memory if needed.
     */
    if (options != NULL)
      efree(options);
    if (p_printer != NULL)
      efree(p_printer); 
    if (p_filename != NULL)
      efree(p_filename); 
    if (p_title != NULL)
      efree(p_title); 

    /*
     *  Return the job id or error.
     */
    RETURN_LONG(ret_val);
}



/*
 * End of "$Id: phpcups.c,v 1.1.2.1 2002/08/19 19:42:18 mike Exp $".
 */
