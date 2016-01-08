---
title: How To Restrict Group Access To A Printer
layout: post
---

Two ways of restricting groups from accessing a printer1. The lpadmin commandThe first way and most popular is to use the lpadmin command. The -u option controls which groups can print to a printer. The default configuration allows all groups to print to a printer:
       /usr/sbin/lpadmin -p printer -u allow:all Along with your list of groups, you can specify whether they are allowed or not allowed to use the printer: The below command allows staff, students to print to the named printer, but all other users cannot print.
      /usr/sbin/lpadmin -p printer -u allow:@staff,@students The following command has the opposite affect:
      /usr/sbin/lpadmin -p printer -u deny:@staff,@students All groups except staff and students will be able to print to the named printer.The allow and deny options are not cummulative. That is, you must provide the complete list of groups to allow or deny each time. Also, CUPS only maintains one list of groups - the list can allow or deny groups from printing. If you specify an allow list and then specify a deny list, the deny list will replace the allow list - only one list is active at any time.
 2. Edit the /etc/cups/printers.conf file  The second method is similar to the first since it changes the same configuration file. Instead of issuing the lpadmin command, edit the /etc/cups/printers.conf file to add groups to the AllowUser and DenyUser directives within a printer definition. Here are two examples: 

 # Deny everyone except for groups staff and students
 <Printer my_printer>
 ...
 AllowUser @staff
 AllowUser @students
 </Printer>
 
 # Accept everyone except for groups staff and students
 <Printer my_printer>
 ...
 DenyUser  @staff
 DenyUser  @students
 </Printer>
  To enable the changes in the configuration file, restart the cupsd daemon.
