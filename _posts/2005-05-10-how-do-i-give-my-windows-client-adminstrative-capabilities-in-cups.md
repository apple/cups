---
title: How Do I Give My Windows Client Adminstrative Capabilities In CUPS?
layout: post
---

From your CUPS server:
1) Goto the text file /etc/cups/cupsd.conf 
2) Scroll down the file and put under the <Location/ admin> section: Allow from windowsclientaddressNow you can use the web interface (http://localhost:631/) with Windows to do administrative tasks.
