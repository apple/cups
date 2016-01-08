---
title: Why Is CUPS Taking Up A Lot Of CPU Memory And How Can I Fix It?
layout: post
---

Number 1: Job History Set Up To Keep Too Many DocumentsA common error is the MaxJobs parameters found in /etc/cups/cupsd.conf is being set to 0. What this does is disable the MaxJobs feature, allowing an unlimited amount of files to be kept. If jobs are not purged, a lot of CPU memory is being used. How do I fix this?Access your /etc/cups/cupsd.conf file and change the MaxJob to a different number. The default setting is 500. Number 2: Improperly Configured Printer Sharing BrowseInterval 
Let's say you have 500 printers on your network. You set your Browse Interval to 30. You are forcing 17 printers to be sent every second. Not very efficient.   A good rule of thumb:  Set your BrowseInterval greater than the number of printers.   BrowseTimeout  The BrowseTimeout directive sets the timeout for printer or class information that is received in browse packets. Once a printer or class times out it is removed from the list of available destinations.    A good rule of thumb  BrowseTimeout should be at least twice the amount entered in the BrowseInterval (3 times is even better).  If not, printers and classes will disappear from client systems between updates.
