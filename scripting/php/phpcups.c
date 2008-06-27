/*
 * "$Id: phpcups.c 7624 2008-06-09 15:55:04Z mike $"
 *
 *   Printing utilities for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007 by Apple Inc.
 *   Copyright 1997-2007 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   which should have been included with this file.  If this file is
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 * Contents:
 *
 *   cups_convert_options()        - Convert a PHP options array to a CUPS options array.
 *   zm_startup_phpcups()          - Initialize the CUPS module.
 *   zif_cups_cancel_job()         - Cancel a job.
 *   zif_cups_get_dests()          - Get a list of printers and classes.
 *   zif_cups_get_jobs()           - Get a list of jobs.
 *   zif_cups_last_error()         - Return the last IPP status code.
 *   zif_cups_last_error_string()  - Return the last IPP status
 *   zif_cups_print_file()         - Print a single file.
 *   zif_cups_print_files()        - Print multiple files.
 */

/*
 * Include necessary headers...
 */

#include <cups/string.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "phpcups.h"


/* 
 * PHP function list...
 */

function_entry phpcups_functions[] =
{
  PHP_FE(cups_cancel_job, NULL)
  PHP_FE(cups_get_dests, NULL)
  PHP_FE(cups_get_jobs, NULL)
  PHP_FE(cups_last_error, NULL)
  PHP_FE(cups_last_error_string, NULL)
  PHP_FE(cups_print_file, NULL)
  PHP_FE(cups_print_files, NULL)
  {NULL, NULL, NULL}
};


/*
 * PHP module info...
 */

zend_module_entry phpcups_module_entry =
{
  STANDARD_MODULE_HEADER,
  "phpcups",
  phpcups_functions,
  PHP_MINIT(phpcups),
  NULL,
  NULL,
  NULL,
  NULL,
  CUPS_SVERSION,
  STANDARD_MODULE_PROPERTIES
};


ZEND_GET_MODULE(phpcups)


/*
 * 'cups_convert_options()' - Convert a PHP options array to a CUPS options array.
 */

static int				/* O - Number of options */
cups_convert_options(
    zval          *optionsobj,		/* I - Options array object */
    cups_option_t **options)		/* O - Options */
{
  int		num_options;		/* Number of options */
  HashTable	*ht;			/* Option array hash table */
  Bucket	*current;		/* Current element in array */
  zval		*value;			/* Current value in array */
  char		temp[255];		/* String value for numbers */


  ht          = Z_ARRVAL_P(optionsobj);
  num_options = 0;

  for (current = ht->pListHead; current; current = current->pListNext)
  {
    value = (zval *)current->pDataPtr;

    switch (Z_TYPE_P(value))
    {
      case IS_LONG :
          sprintf(temp, "%ld", Z_LVAL_P(value));
          num_options = cupsAddOption(current->arKey, temp, num_options,
	                              options);
          break;

      case IS_DOUBLE :
          sprintf(temp, "%g", Z_DVAL_P(value));
          num_options = cupsAddOption(current->arKey, temp, num_options,
	                              options);
          break;

      case IS_BOOL :
          num_options = cupsAddOption(current->arKey,
	                              Z_BVAL_P(value) ? "true" : "false",
				      num_options, options);
          break;

      case IS_STRING :
          num_options = cupsAddOption(current->arKey, Z_STRVAL_P(value),
				      num_options, options);
          break;
    }
  }

  return (num_options);
}


/*
 * 'zm_startup_phpcups()' - Initialize the CUPS module.
 */

PHP_MINIT_FUNCTION(phpcups)
{
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_LOCAL", CUPS_PRINTER_LOCAL, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_CLASS", CUPS_PRINTER_CLASS, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_REMOTE", CUPS_PRINTER_REMOTE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_BW", CUPS_PRINTER_BW, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_COLOR", CUPS_PRINTER_COLOR, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_DUPLEX", CUPS_PRINTER_DUPLEX, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_STAPLE", CUPS_PRINTER_STAPLE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_COPIES", CUPS_PRINTER_COPIES, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_COLLATE", CUPS_PRINTER_COLLATE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_PUNCH", CUPS_PRINTER_PUNCH, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_COVER", CUPS_PRINTER_COVER, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_BIND", CUPS_PRINTER_BIND, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_SORT", CUPS_PRINTER_SORT, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_SMALL", CUPS_PRINTER_SMALL, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_MEDIUM", CUPS_PRINTER_MEDIUM, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_LARGE", CUPS_PRINTER_LARGE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_VARIABLE", CUPS_PRINTER_VARIABLE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_IMPLICIT", CUPS_PRINTER_IMPLICIT, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_DEFAULT", CUPS_PRINTER_DEFAULT, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_FAX", CUPS_PRINTER_FAX, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_REJECTING", CUPS_PRINTER_REJECTING, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_DELETE", CUPS_PRINTER_DELETE, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_NOT_SHARED", CUPS_PRINTER_NOT_SHARED, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_AUTHENTICATED", CUPS_PRINTER_AUTHENTICATED, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_COMMANDS", CUPS_PRINTER_COMMANDS, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_DISCOVERED", CUPS_PRINTER_DISCOVERED, CONST_CS);
  REGISTER_LONG_CONSTANT("CUPS_PRINTER_OPTIONS", CUPS_PRINTER_OPTIONS, CONST_CS);

  REGISTER_LONG_CONSTANT("IPP_OK", IPP_OK, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_SUBST", IPP_OK_SUBST, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_CONFLICT", IPP_OK_CONFLICT, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_IGNORED_SUBSCRIPTIONS", IPP_OK_IGNORED_SUBSCRIPTIONS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_IGNORED_NOTIFICATIONS", IPP_OK_IGNORED_NOTIFICATIONS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_TOO_MANY_EVENTS", IPP_OK_TOO_MANY_EVENTS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_BUT_CANCEL_SUBSCRIPTION", IPP_OK_BUT_CANCEL_SUBSCRIPTION, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OK_EVENTS_COMPLETE", IPP_OK_EVENTS_COMPLETE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_REDIRECTION_OTHER_SITE", IPP_REDIRECTION_OTHER_SITE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_BAD_REQUEST", IPP_BAD_REQUEST, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_FORBIDDEN", IPP_FORBIDDEN, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_NOT_AUTHENTICATED", IPP_NOT_AUTHENTICATED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_NOT_AUTHORIZED", IPP_NOT_AUTHORIZED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_NOT_POSSIBLE", IPP_NOT_POSSIBLE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_TIMEOUT", IPP_TIMEOUT, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_NOT_FOUND", IPP_NOT_FOUND, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_GONE", IPP_GONE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_REQUEST_ENTITY", IPP_REQUEST_ENTITY, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_REQUEST_VALUE", IPP_REQUEST_VALUE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_DOCUMENT_FORMAT", IPP_DOCUMENT_FORMAT, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_ATTRIBUTES", IPP_ATTRIBUTES, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_URI_SCHEME", IPP_URI_SCHEME, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_CHARSET", IPP_CHARSET, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_CONFLICT", IPP_CONFLICT, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_COMPRESSION_NOT_SUPPORTED", IPP_COMPRESSION_NOT_SUPPORTED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_COMPRESSION_ERROR", IPP_COMPRESSION_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_DOCUMENT_FORMAT_ERROR", IPP_DOCUMENT_FORMAT_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_DOCUMENT_ACCESS_ERROR", IPP_DOCUMENT_ACCESS_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_ATTRIBUTES_NOT_SETTABLE", IPP_ATTRIBUTES_NOT_SETTABLE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_IGNORED_ALL_SUBSCRIPTIONS", IPP_IGNORED_ALL_SUBSCRIPTIONS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_TOO_MANY_SUBSCRIPTIONS", IPP_TOO_MANY_SUBSCRIPTIONS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_IGNORED_ALL_NOTIFICATIONS", IPP_IGNORED_ALL_NOTIFICATIONS, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_PRINT_SUPPORT_FILE_NOT_FOUND", IPP_PRINT_SUPPORT_FILE_NOT_FOUND, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_INTERNAL_ERROR", IPP_INTERNAL_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_OPERATION_NOT_SUPPORTED", IPP_OPERATION_NOT_SUPPORTED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_SERVICE_UNAVAILABLE", IPP_SERVICE_UNAVAILABLE, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_VERSION_NOT_SUPPORTED", IPP_VERSION_NOT_SUPPORTED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_DEVICE_ERROR", IPP_DEVICE_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_TEMPORARY_ERROR", IPP_TEMPORARY_ERROR, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_NOT_ACCEPTING", IPP_NOT_ACCEPTING, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_PRINTER_BUSY", IPP_PRINTER_BUSY, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_ERROR_JOB_CANCELLED", IPP_ERROR_JOB_CANCELLED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_MULTIPLE_JOBS_NOT_SUPPORTED", IPP_MULTIPLE_JOBS_NOT_SUPPORTED, CONST_CS);
  REGISTER_LONG_CONSTANT("IPP_PRINTER_IS_DEACTIVATED", IPP_PRINTER_IS_DEACTIVATED, CONST_CS);

  return (SUCCESS);
}

/*
 * 'zif_cups_cancel_job()' - Cancel a job.
 */

PHP_FUNCTION(cups_cancel_job)
{
  char	*dest;				/* Destination */
  int	dest_len,			/* Length of destination */
	id;				/* Job ID */


  if (ZEND_NUM_ARGS() != 2 ||
      zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &dest, &dest_len, &id))
  {
    WRONG_PARAM_COUNT;
  }

  RETURN_LONG(cupsCancelJob(dest, id));
}


/*
 * 'zif_cups_get_dests()' - Get a list of printers and classes.
 */

PHP_FUNCTION(cups_get_dests)
{
  int		i, j,			/* Looping vars */
		num_dests;		/* Number of destinations */
  cups_dest_t	*dests,			/* Destinations */
		*dest;			/* Current destination */
  cups_option_t	*option;		/* Current option */
  zval		*destobj,		/* Destination object */
		*optionsobj;		/* Options object */


  if (ZEND_NUM_ARGS() != 0)
  {
    WRONG_PARAM_COUNT;
  }

  if ((num_dests = cupsGetDests(&dests)) <= 0)
  {
    RETURN_NULL();
  }

  if (array_init(return_value) == SUCCESS)
  {
    for (i = 0, dest = dests; i < num_dests; i ++, dest ++)
    {
      MAKE_STD_ZVAL(destobj);

      if (object_init(destobj) == SUCCESS)
      {
       /*
        * Add properties to the destination for each of the cups_dest_t
	* members...
	*/

        add_property_string(destobj, "name", dest->name, 1);
        add_property_string(destobj, "instance",
	                    dest->instance ? dest->instance : "", 1);
        add_property_long(destobj, "is_default", dest->is_default);

       /*
        * Create an associative array for the options...
	*/

        MAKE_STD_ZVAL(optionsobj);

	if (array_init(optionsobj) == SUCCESS)
	{
	  for (j = 0, option = dest->options;
	       j < dest->num_options;
	       j ++, option ++)
	    add_assoc_string(optionsobj, option->name, option->value, 1);

	  add_property_zval(destobj, "options", optionsobj);
	}

        add_index_zval(return_value, i, destobj);
      }
    }
  }

  cupsFreeDests(num_dests, dests);
}


/*
 * 'zif_cups_get_jobs()' - Get a list of jobs.
 */

PHP_FUNCTION(cups_get_jobs)
{
  char		*dest;			/* Destination */
  int		dest_len,		/* Length of destination */
		myjobs,			/* Only show my jobs? */
		completed;		/* Show completed jobs? */
  int		i,			/* Looping var */
		num_jobs;		/* Number of jobs */
  cups_job_t	*jobs,			/* Jobs */
		*job;			/* Current job */
  zval		*jobobj;		/* Job object */




  if (ZEND_NUM_ARGS() != 3 ||
      zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sll", &dest, &dest_len, &myjobs, &completed))
  {
    WRONG_PARAM_COUNT;
  }

  if (!*dest)
    dest = NULL;

  if ((num_jobs = cupsGetJobs(&jobs, dest, myjobs, completed)) <= 0)
  {
    RETURN_NULL();
  }

  if (array_init(return_value) == SUCCESS)
  {
    for (i = 0, job = jobs; i < num_jobs; i ++, job ++)
    {
      MAKE_STD_ZVAL(jobobj);

      if (object_init(jobobj) == SUCCESS)
      {
       /*
        * Add properties to the job for each of the cups_job_t
	* members...
	*/

        add_property_long(jobobj, "id", job->id);
        add_property_string(jobobj, "dest", job->dest, 1);
        add_property_string(jobobj, "title", job->title, 1);
        add_property_string(jobobj, "user", job->user, 1);
        add_property_string(jobobj, "format", job->format, 1);
        add_property_long(jobobj, "state", job->state);
        add_property_long(jobobj, "size", job->size);
        add_property_long(jobobj, "priority", job->priority);
        add_property_long(jobobj, "completed_time", job->completed_time);
        add_property_long(jobobj, "creation_time", job->creation_time);
        add_property_long(jobobj, "processing_time", job->processing_time);

        add_index_zval(return_value, i, jobobj);
      }
    }
  }

  cupsFreeJobs(num_jobs, jobs);
}


/*
 * 'zif_cups_last_error()' - Return the last IPP status code.
 */

PHP_FUNCTION(cups_last_error)
{
  if (ZEND_NUM_ARGS() != 0)
  {
    WRONG_PARAM_COUNT;
  }

  RETURN_LONG(cupsLastError());
}


/*
 * 'zif_cups_last_error_string()' - Return the last IPP status-message.
 */

PHP_FUNCTION(cups_last_error_string)
{
  if (ZEND_NUM_ARGS() != 0)
  {
    WRONG_PARAM_COUNT;
  }

  RETURN_STRING((char *)cupsLastErrorString(), 1);
}


/*
 * 'zif_cups_print_file()' - Print a single file.
 */

PHP_FUNCTION(cups_print_file)
{
  char		*dest;			/* Destination */
  int		dest_len;		/* Length of destination */
  char		*filename;		/* Filename */
  int		filename_len;		/* Length of filename */
  char		*title;			/* Title */
  int		title_len;		/* Length of title */
  zval		*optionsobj;		/* Array of options */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  int		id;			/* Job ID */


  if (ZEND_NUM_ARGS() != 4 ||
      zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssa", &dest, &dest_len,
                            &filename, &filename_len,
			    &title, &title_len, &optionsobj))
  {
    WRONG_PARAM_COUNT;
  }

  num_options = cups_convert_options(optionsobj, &options);

  id = cupsPrintFile(dest, filename, title, num_options, options);

  cupsFreeOptions(num_options, options);

  RETURN_LONG(id);
}


/*
 * 'zif_cups_print_files()' - Print multiple files.
 */

PHP_FUNCTION(cups_print_files)
{
  char		*dest;			/* Destination */
  int		dest_len;		/* Length of destination */
  zval		*filesobj;		/* Files array */
  int		num_files;		/* Number of files */
  const char	*files[1000];		/* Files */
  char		*title;			/* Title */
  int		title_len;		/* Length of title */
  zval		*optionsobj;		/* Array of options */
  int		num_options;		/* Number of options */
  cups_option_t	*options;		/* Options */
  HashTable	*ht2;			/* Option array hash table */
  Bucket	*current;		/* Current element in array */
  int		id;			/* Job ID */


  if (ZEND_NUM_ARGS() != 4 ||
      zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sasa", &dest, &dest_len, &filesobj,
			    &title, &title_len, &optionsobj))
  {
    WRONG_PARAM_COUNT;
  }

  ht2       = Z_ARRVAL_P(filesobj);
  num_files = 0;

  for (current = ht2->pListHead; current; current = current->pListNext)
  {
    files[num_files ++] = Z_STRVAL_P(((zval *)current->pDataPtr));

    if (num_files >= (int)(sizeof(files) / sizeof(files[0])))
      break;
  }

  num_options = cups_convert_options(optionsobj, &options);

  id = cupsPrintFiles(dest, num_files, files, title, num_options, options);

  cupsFreeOptions(num_options, options);

  RETURN_LONG(id);
}


/*
 * End of "$Id: phpcups.c 7624 2008-06-09 15:55:04Z mike $".
 */
