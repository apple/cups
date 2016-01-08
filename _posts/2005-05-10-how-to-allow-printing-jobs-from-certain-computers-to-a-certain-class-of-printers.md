---
title: How To Allow Printing Jobs From Certain Computers To A Certain Class Of Printers
layout: post
---

To allow printing from certain computers to a certain class, edit the /etc/cups/cupsd.conf file and see if the <Location /classes/class_name> is already present. Otherwise, create it with these two lines:

 <Location /printers/class_name>
 </Location>
The class_name portion represents the printer class name that you have configured already. Between the above two lines, you can add or edit the Order, Allow, and Deny directives to your liking. Here are two examples:

 # Allow printing to class MyLaserJet_class from 
 #    itself and computers 192.10.2.5 and 192.10.2.6
 <Location /classes/MyLaserJet_class>
 ...
 Order deny,allow
 Deny from all
 Allow from 127.0.0.1
 Allow from 192.10.2.5
 Allow from 192.10.2.6
 </Location>
 
 # Allow printing to class MyLaserJet_class from 
 #    itself and all computers on subnet 192.10.2.x
 <Location /classes/MyLaserJet_class>
 ...
 Order deny,allow
 Deny from all
 Allow from 127.0.0.1
 Allow from 192.10.2.0/255.255.255.0
 </Location>
To enable the changes in the configuration file, restart the cupsd daemon.
