---
title: Common UNIX Printing System 1.3.3
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

CUPS 1.3.3 is now available for download from the CUPS web site and fixes some scheduler and localization issues. Changes include:
- The scheduler did not use the attributes-natural-language attribute when passing the LANG environment variable to cups-deviced or cups-driverd.
- The scheduler did not use the printer-op-policy when modifying classes or printers (Issue #2525)
- The auth-info-required attribute was not always updated for remote queues that required authentication.
- The German web interface localization contained errors (Issue #2523)
- The Swedish localization contained errors (Issue #2522)
