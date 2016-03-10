---
title: How To Allow Printing Jobs From Certain Computers
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

To allow printing from certain computers, edit the /etc/cups/cupsd.conf file and find the <Location /> directive. There you can add or edit the  Order, Allow, and  Deny directives to your liking. Here are two examples:

 # Allow printing from 
 #    itself and computers 192.10.2.5 and 192.10.2.6
 <Location />
 ...
 Order deny,allow
 Deny from all
 Allow from 127.0.0.1
 Allow from 192.10.2.5
 Allow from 192.10.2.6
 </Location>
 
 # Allow printing from 
 #    itself and all computers on subnet 192.10.2.x
 <Location />
 ...
 Order deny,allow
 Deny from all
 Allow from 127.0.0.1
 Allow from 192.10.2.0/255.255.255.0
 </Location>
To enable the changes in the configuration file, restart the cupsd daemon.
