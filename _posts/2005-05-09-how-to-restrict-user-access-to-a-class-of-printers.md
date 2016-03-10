---
title: How To Restrict User Access To A Class Of Printers
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Two ways of restricting users from accessing a class of printers1. The lpadmin commandThe first way and most popular is to use the lpadmin command. The -u option controls which users can print to a class of printers. The default configuration allows all users to print to a class of printers:
    /usr/sbin/lpadmin -p class -u allow:all Along with your list of users, you can specify whether they are allowed or not allowed to use the class of printers: The below command allows peter, paul, and mary to print to the named class, but all other users cannot print.
    /usr/sbin/lpadmin -p class -u allow:peter,paul,maryThe following command has the opposite affect:
    /usr/sbin/lpadmin -p class -u deny:peter,paul,mary All users except peter, paul, and mary will be able to print to the named class.The allow and deny options are not cummulative. That is, you must provide the complete list of users to allow or deny each time. Also, CUPS only maintains one list of users - the list can allow or deny users from printing. If you specify an allow list and then specify a deny list, the deny list will replace the allow list - only one list is active at any time.2. Edit the /etc/cups/classes.conf fileThe second method is similar to the first since it changes the same configuration file. Instead of issuing the lpadmin command, edit the /etc/cups/classes.conf file to add users to the AllowUser and DenyUser directives within a class definition. For example:

 # Deny everyone except for users peter, paul and mary
 <Class my_class>
 ...
 AllowUser peter
 AllowUser paul
 AllowUser mary
 </Class>
 
 # Accept everyone except for users peter, paul and mary
 <Class my_class>
 ...
 DenyUser  peter
 DenyUser  paul
 DenyUser  mary
 </Class>
To enable the changes in the configuration file, restart the cupsd daemon.
