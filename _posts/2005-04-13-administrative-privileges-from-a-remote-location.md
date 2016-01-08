---
title: Administrative Privileges From A Remote Location
layout: post
---

1) Go to the text file /etc/cups/cupsd.conf
2) Scroll down the file and put under the <Location/ admin> section: allow from (whatever machine you want it to pertain to)Just add an Allow directive for every computer you want to be able to do administrative duties on.

Here is an example:I want to allow remote access to another computer (IP address is 192.168.10.1).When I access the text file /etc/cups/cupsd.conf, I get a long list of information. I scroll down until I find the section: <Location /admin>.It looks like this:

 <Location /admin>
 Order deny,allow
 Encryption IfRequested
 Satisfy All
 AuthType Basic
 AuthClass System
 Deny All
 Allow 127.0.0.1
 </Location>
How do I allow remote access to my other computer?Add another Allow line.Your updated file should look like this:

 <Location /admin>
 Order deny,allow
 Encryption IfRequested
 Satisfy All
 AuthType Basic
 AuthClass System
 Deny All
 Allow 127.0.0.1
 Allow 192.168.10.1
 </Location>
