---
title: How To Restrict Printer Information Being Received From A Client Or Server
layout: post
---

Printer browsing allows your server to automatically share its printers with client machines and other servers. To make sure printer information only comes from certain servers and clients, edit the /etc/cups/cupsd.conf file. Locate the BrowseOrder, BrowseAllow and BrowseDeny directives and then edit or add IP or netmask addresses to your liking. Some examples:

 # Deny from everybody except for servers 192.10.2.3 and 192.10.2.5
 
 BrowseOrder deny,allow
 BrowseDeny  from all
 BrowseAllow 192.10.2.3
 BrowseAllow 192.10.2.5
 
 # Deny from everybody except for servers under 192.10.2.x
 
 BrowseOrder deny,allow
 BrowseDeny  from all
 BrowseAllow 192.10.2.0/255.255.255.0
To enable the changes in the configuration file, restart the cupsd daemon.
