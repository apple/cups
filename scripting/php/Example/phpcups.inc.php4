<?php
/*
 *   PHP module for the Common UNIX Printing System (CUPS).
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
 *
 * Contents:
 *
 *  Basic CupsPrinter class.  Will be extended in next release.
 *
 */
if (!$INCLUDED_PHPCUPS_INC)
{
  $INCLUDED_PHPCUPS_INC = True;


  class CupsPrinter
  {
    var $printer_name;
    var $printer_server;
    var $printer_instance;
    var $is_default;
    var $printer_options;
    var $printer_state;
    var $printer_state_message;
    var $accepting_jobs;
    var $queued_job_count;
    var $description;
    var $location;
    var $printer_info;
    var $printer_more_info;
    var $make_and_model;
    var $printer_uri;
    var $device_uri;
    var $job_quota_period;
    var $job_k_limit;
    var $job_page_limit;
    var $color_supported;
    var $pages_per_minute;
    var $finishings_supported;             // Array
    var $finishings_default;
    var $printer_type;
    var $operations_supported;             // Array
    var $multiple_document_jobs_supported;
    var $multiple_operation_time_out;
    var $job_priority_supported_lower;
    var $job_priority_supported_upper;
    var $job_priority_default;
    var $copies_supported_lower;
    var $copies_supported_upper;
    var $copies_default;
    var $page_range_supported;
    var $number_up_supported;              // Array
    var $number_up_default;
    var $orientation_requested_supported;  // Array
    var $orientation_requested_default;
    var $media_supported;                  // Array
    var $media_default;

    function CupsPrinter()
    {
      $this->printer_name = "";
      $this->printer_destination = "";
      $this->is_default = 0;
      $this->options = Array();
      $this->printer_state   = -1;
      $this->printer_state_message = "";
      $this->accepting_jobs = FALSE;
      $this->queued_job_count = 0;
      $this->description = "";
      $this->location = "";
      $this->printer_info = "";
      $this->printer_more_info = "";
      $this->make_and_model = "";
      $this->printer_uri_supported = Array();
      $this->device_uri  = "";
      $this->job_quota_period = 0;
      $this->job_k_limit = 0;
      $this->job_page_limit = 0;
      $this->color_supported = FALSE;
      $this->pages_per_minute = 0;
      $this->finishings_supported = Array();
      $this->finishings_default = 0;
      $this->printer_type = 0;
      $this->operations_supported = Array();
      $this->multiple_document_jobs_supported = FALSE;
      $this->multiple_operation_time_out = 0;
      $this->job_priority_supported_lower = 0;
      $this->job_priority_supported_upper = 100;
      $this->job_priority_default = 50;
      $this->copies_supported_lower = 1;
      $this->copies_supported_upper = 1;
      $this->copies_default = 1;
      $this->page_range_supported = FALSE;
      $this->number_up_supported = Array();
      $this->number_up_default = 0;
      $this->orientation_requested_supported = Array();
      $this->orientation_requested_default = 3;
      $this->media_supported = Array();
      $this->media_default = "";

    }  // End of constructor


    //
    //  Get the attributes
    //
    function getAttributes()
    {
      $o_arr = cups_get_dest_options($this->printer_server,
                                     $this->printer_name,
                                     $this->printer_instance);
      $this->printer_options  = $o_arr;

      $attrs = cups_get_printer_attributes( "localhost", $this->printer_name );
      while ($obj = current($attrs))
      {
        next($attrs);

        if ($obj->name == "printer-state")
        {
           $this->printer_state = $obj->value;
        }
        else if ($obj->name == "printer-state-message")
        {
           $this->printer_state_message = $obj->value;
        }
        else if ($obj->name == "printer-location")
        {
           $this->location = $obj->value;
        }
        else if ($obj->name == "printer-make-and-model")
        {
           $this->description = $obj->value;
        }
        else if ($obj->name == "printer-uri-supported")
        {
           $this->printer_uri_supported[$obj->value] = $obj->value;
        }
        else if ($obj->name == "device-uri")
        {
           $this->device_uri = $obj->value;
        }
        else if ($obj->name == "queued-job-count")
        {
           $this->queued_job_count = $obj->value;
        }
        else if ($obj->name == "printer-is-accepting-jobs")
        {
           $this->accepting_jobs = $obj->value ? TRUE : FALSE;
        }
        else if ($obj->name == "color-supported")
        {
           $this->color_supported = $obj->value ? TRUE : FALSE;
        }
        else if ($obj->name == "pages-per-minute")
        {
           $this->pages_per_minute = $obj->value;
        }
        else if ($obj->name == "operations-supported")
        {
           $this->operations_supported["O$obj->value"] = $obj->value;
        }
        else if ($obj->name == "orientation-requested-supported")
        {
           $this->orientation_requested_supported["O$obj->value"] = $obj->value;
        }
        else if ($obj->name == "orientation-requested-default")
        {
           $this->orientation_requested_default = $obj->value;
        }
        else if ($obj->name == "finishings-supported")
        {
           $this->finishings_supported["F$obj->value"] = $obj->value;
        }
        else if ($obj->name == "finishings-default")
        {
           $this->finishings_default = $obj->value;
        }
        else if ($obj->name == "number-up-supported")
        {
           $this->number_up_supported["N$obj->value"] = $obj->value;
        }
        else if ($obj->name == "number-up-default")
        {
           $this->number_up_default = $obj->value;
        }
        else if ($obj->name == "printer-type")
        {
           $this->printer_type = $obj->value;
        }
        else if ($obj->name == "multiple-document-jobs-suppoted")
        {
           $this->multiple_document_jobs_supported = $obj->value ? TRUE : FALSE;
        }
        else if ($obj->name == "multiple-operation-time-out")
        {
           $this->multiple_operation_time_out = $obj->value;
        }
        else if ($obj->name == "job-priority-supported")
        {
           $this->job_priority_supported_upper = $obj->value;
        }
        else if ($obj->name == "job-priority-default")
        {
           $this->job_priority_default = $obj->value;
        }
        else if ($obj->name == "copies-supported")
        {
           $tmpa = explode("-",$obj->value);
           if (count($tmpa) > 1)
           {
             $this->copies_supported_lower = $tmpa[0];
             $this->copies_supported_upper = $tmpa[1];
           }
           else if (count($tmpa) == 1)
           {
             $this->copies_supported_lower = $tmpa[0];
             $this->copies_supported_upper = $tmpa[0];
           }
        }
        else if ($obj->name == "copies-default")
        {
          $this->copies_supported_default = $obj->value;
        }
        else if ($obj->name == "page-ranges-supported")
        {
          $this->page_ranges_supported = $obj->value ? TRUE : FALSE;
        }
        else if ($obj->name == "media-default")
        {
          $this->media_default = $obj->value;
        }
        else if ($obj->name == "media-supported")
        {
          $this->media_supported[$obj->value] = $obj->value;
        }

      } // while

    }  // End of getAttributes


  }  //  End of CupsPrinter class




  //
  //  Get the printer / destination list.
  //
  function phpcups_getDestList()
  {
    $return_value = Array();

    //
    //  Get the destination objects array.
    //
    $p_arr = cups_get_dest_list();

    if (!IS_ARRAY($p_arr))
    {
      return(NULL);
    }

    reset($p_arr);

    while ($p_obj = current($p_arr))
    {
      next($p_arr);

      //
      //  Get the options for the current destination.
      //
      $o_arr = cups_get_dest_options("localhost",$p_obj->name,$p_obj->instance);

      $p = new CupsPrinter();
      $p->printer_name     = $p_obj->name;
      $p->printer_instance = $p_obj->instance;
      $p->is_default       = $p_obj->is_default;
      $p->printer_options  = $o_arr;
      $p->getAttributes();

      $return_value[$p->printer_name] = $p;
    }
    return($return_value);

  } // End of phpcups_getDestList()




}   // if included.
?>
