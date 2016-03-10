---
title: Printing To A Windows Server With LPD
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

On the Windows Server:Start up "TCP/IP Printing Server":1) click Start -> Control Panel -> Administrative Tools -> Services2) Scoll down the list shown and double-click on "TCP/IP Printing Server"3) Go down to the Startup type field and change Manual to Automatic4) Click Start ButtonOn the CUPS system:Add a printer using the device URI of:  lpd://server/name Replace server with the hostname or IP address of the server and name with the queue name.
