#!/usr/bin/php -f
<?
//
// "$Id: phpcups.php 3603 2003-04-11 18:42:52Z mike $"
//
//   PHP test script for the Common UNIX Printing System (CUPS).
//
//   Copyright 2007 by Apple Inc.
//   Copyright 1997-2006 by Easy Software Products, all rights reserved.
//
//   These coded instructions, statements, and computer programs are the
//   property of Apple Inc. and are protected by Federal copyright
//   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
//   which should have been included with this file.  If this file is
//   file is missing or damaged, see the license at "http://www.cups.org/".
//

// Make sure the module is loaded...
if(!extension_loaded("phpcups"))
{
  dl("phpcups.so");
}

// Get the list of functions in the module...
$module    = "phpcups";
$functions = get_extension_funcs($module);

print("Functions available in the $module extension:\n");

foreach ($functions as $func)
{
  print("$func\n");
}

print("\n");

print("cups_get_dests:\n");
print_r(cups_get_dests());

print("cups_get_jobs(\"\", 0, -1):\n");
print_r(cups_get_jobs("", 0, -1));

print("cups_print_file(\"test\", \"../../test/testfile.jpg\", "
     ."\"testfile.jpg\", ...):\n");
print_r(cups_print_file("test", "../../test/testfile.jpg", "testfile.jpg",
                        array("scaling" => "100",
			      "page-label" => "testfile.jpg")));

print("cups_print_files(\"test\", array(\"../../test/testfile.jpg\", "
     ."\"../../test/testfile.ps\"), \"testfiles\", ...):\n");
print_r(cups_print_files("test", array("../../test/testfile.jpg",
                                       "../../test/testfile.ps"),
                         "testfiles",
                         array("scaling" => "100",
			       "page-label" => "testfile.jpg")));

//
// End of "$Id: phpcups.php 3603 2003-04-11 18:42:52Z mike $".
//
?>
