---
title: How Do I Fix The Error&#58; "client-error-request-value-too-long"
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

 Here is a quick check list to get you on your way:


   1. Are you trying to print a file >2GB?  If so, that doesn't
       work in CUPS 1.1.x and earlier.

    2. Does the RequestRoot directory (/var/spool/cups by default)
       exist?  If not, "mkdir /var/spool/cups"

    3. Does the TempDir directory (/var/spool/cups/tmp by default)
       exist?  If not, "mkdir /var/spool/cups/tmp"

    4. Is the disk full?  "df -k /var/spool/cups" will show if
       this is the case.  If the disk is full, delete files to
       free up disk space.


