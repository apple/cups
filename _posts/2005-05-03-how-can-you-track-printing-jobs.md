---
title: How Can You Track Printing Jobs?
layout: post
---

The easiest way to track your jobs is from the web interface located at: http://localhost:631/adminJust click on the printer you are interested in and see who has used the printer and see all completed and uncompleted jobs.
 Tracking Jobs From The Command Line If you want to see the status of any pending job: lpstat If you want to see all your completed jobs: lpstat -W completed If you want to see all the incomplete jobs: lpstat -W not-completed If you want to see all completed jobs: lpstat -W completed -o If you want to see your printing activity on a particular printer: lpstat printernameSo to see your activiy on printer N2025, you'd type: lpstat N2025 To see everyone's activty on a printer: lpstat printername -oAs shown before, printername must be replaced by the name of your printer.Note:For a full explanation of the lpstat command type: man lpstat
