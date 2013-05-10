<?
//
// "$Id: phpcups.php 3603 2003-04-11 18:42:52Z mike $"
//
//   PHP test script for the Common UNIX Printing System (CUPS).
//
//   Copyright 1997-2003 by Easy Software Products, all rights reserved.
//
//   These coded instructions, statements, and computer programs are the
//   property of Easy Software Products and are protected by Federal
//   copyright law.  Distribution and use rights are outlined in the file
//   "LICENSE.txt" which should have been included with this file.  If this
//   file is missing or damaged please contact Easy Software Products
//   at:
//
//       Attn: CUPS Licensing Information
//       Easy Software Products
//       44141 Airport View Drive, Suite 204
//       Hollywood, Maryland 20636-3111 USA
//
//       Voice: (301) 373-9603
//       EMail: cups-info@cups.org
//         WWW: http://www.cups.org
//

// Make sure the module is loaded...
if(!extension_loaded('phpcups')) {
	dl('phpcups.so');
}

// Get the list of functions in the module...
$module    = 'phpcups';
$functions = get_extension_funcs($module);

echo "Functions available in the $module extension:<br>\n";

foreach($functions as $func) {
    echo $func."<br>\n";
}
echo "<br>\n";

$function = 'confirm_' . $module . '_compiled';
if (extension_loaded($module)) {
	$str = $function($module);
} else {
	$str = "Module $module is not compiled into PHP";
}
echo "$str\n";

//
// End of "$Id: phpcups.php 3603 2003-04-11 18:42:52Z mike $".
//
?>
