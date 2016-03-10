---
title: Absolute Basics For Setting Up Your Printing Network
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

Setting Up Your Client And Your ServerAll the information below can be entered in the cupsd.conf file with your favorite text editor.For example, if I was using the text editor nedit, I'd type: nedit /etc/cups/cupsd.conf1) Browsing must be allowed in both the client and the serverBy default, CUPS allows browsing. (Redhat, SUSE are turned off by default). So both the client and the server need to be set up to accept browsing.To accept browsing, scroll down to the Browsing directive in your cupsd.conf file and change the Off to On.It should now read Browsing On.2) Make sure the Server is listening to the ClientBy default, this should already be set as:
Listen *:631This setting will listen to any address on that network. This is not the default setting for Redhat or SUSE. If you only wanted it to listen to one network address, you could replace the * with an actual address.  For example, Listen 127.0.0.0:6313) Set up who is allowed to be access the ServerMake sure Allow directive looks like this:
Allow from @LOCALThe default setting for this is: 
Allow from 127.0.0.1Why set up as @LOCAL?Eliminates the need to know other client addresses on the local network 4) Enable broadcasting from your server 
BrowseAddress @LOCALNo browse addresses are set by defaultWhy set up as @LOCAL?Eliminates the need to know other client addresses on the local network
