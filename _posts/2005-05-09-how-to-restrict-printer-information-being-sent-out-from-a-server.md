---
title: How To Restrict Printer Information Being Sent Out From A Server
layout: post
---

Printer browsing allows your server to automatically share its printers with client machines and other servers. To make sure that this printer information is sent to the right computers, edit the /etc/cups/cupsd.conf file. Locate the BrowseAddress directive and then edit or add IP or netmask addresses to your liking. Some examples:

 # Broadcasts to all computers through all interfaces
 
 BrowseAddress 255.255.255.255:631
 
 # Broadcast to all computers on the 192.0.2.x network
 
 BrowseAddress 192.0.2.255:631
 
 # Broadcast to computers 192.0.2.14 and 192.0.2.15
 
 BrowseAddress 192.0.2.14:631
 BrowseAddress 192.0.2.15:631
 
 # Specific hostname address (must enable HostNameLookups)
 
 BrowseAddress host.domain.com:631
The BrowseAddress directive specifies an address to send browsing information to. Multiple BrowseAddress directives can be specified to send browsing information to different networks or systems.The default address is 255.255.255.255:631 which will broadcast the information to all networks the server is connected to. This global setting might have the undesired effect of causing your PC dialing into your ISP account for every broadcast occurance. To block this, choose valid broadcast address(es) for your LAN(s) only.To enable the changes in the configuration file, restart the cupsd daemon.
