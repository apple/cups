---
title: How To Restrict User Access To A Printer
layout: post
---

Two Ways To Restrict User Access To A Printer1) Use the lpadmin command 
 The first way and most popular is to use the lpadmin command. The -u option controls which users can print to a printer. The default configuration allows all users to print to a printer:  
  /usr/sbin/lpadmin -p printer -u allow:all   Along with your list of users, you can specify whether they are allowed or not allowed to use the printer: The below command allows peter, paul, and mary to print to the named printer, but all other users cannot print. 
 /usr/sbin/lpadmin -p printer -u allow:peter,paul,mary   The following command has the opposite affect:  
 /usr/sbin/lpadmin -p printer -u deny:peter,paul,mary   All users except peter, paul, and mary will be able to print to the named printer.  The allow and deny options are not cummulative. That is, you must provide the complete list of users to allow or deny each time. Also, CUPS only maintains one list of users - the list can allow or deny users from printing. If you specify an allow list and then specify a deny list, the deny list will replace the allow list - only one list is active at any time. 
 2) Edit /etc/cups/printers.conf file 
The second method is similar to the first since it changes the same configuration file. Instead of issuing the lpadmin command, edit the /etc/cups/printers.conf file to add users to the AllowUser and DenyUser directives within a printer definition. Here are two examples: 

 # Deny everyone except for users peter, paul and mary
 <Printer my_printer>
 ...
 AllowUser peter
 AllowUser paul
 AllowUser mary
 </Printer>
 
 # Accept everyone except for users peter, paul and mary
 <Printer my_printer>
 ...
 DenyUser  peter
 DenyUser  paul
 DenyUser  mary
 </Printer>
        To enable the changes in the configuration file, restart the cupsd daemon.
