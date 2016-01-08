---
title: Why Am I No Longer Able To Control My Printing System?
layout: post
---

Your printing system is acting funny and you don't know why. You can't access printers, you can't use the web interface, you aren't allowed remote access, everything your system should be doing, it's not. All the settings you've made in the cupsd.conf file are being erased. Your printing system has gone on strike. What do you do?This is usually caused by the Linux cups-config-daemon program, which overwrites changes to the /etc/cups/cupsd.conf file with whatever defaults they have assigned for your security configuration.Run the following commands as root to disable this program:/etc/init.d/cups-config-daemon stop ENTER
/sbin/chkconfig cups-config-daemon off ENTER
