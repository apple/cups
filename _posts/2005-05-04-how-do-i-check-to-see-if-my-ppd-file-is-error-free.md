---
title: How Do I Check To See If My PPD File Is Error Free?
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

PPD files can be tested on-line or from the command line using the cupstestppd program. If you are experiencing difficulties with a printer, use this page to validate the PPD file you are using and report any problems to the author of the file/driver. Where Do I Find This Program? To access online:

 1) From the homepage click the  Printer Drivers  button 
 2) Click  Test PPD file 
 3) Type the filename or hit browse to find the filename you desire
 4) Hit  Test PPD File 
 To access from the command-line:  /usr/bin/cupstestppdthen... cupstestppd filename Note: The cupstestppd program is available on CUPS version 1.17 and higher. The Test Results: The cupstestppd program returns a zero for a functional driver and a non-zero for a file containing errors.
The error codes are as follows:

 1
     Bad command-line arguments or missing PPD filename. 
 2
     Unable to open or read PPD file. 
 3
     The PPD file contains format errors that cannot be skipped. 
 4
     The PPD file does not conform to the Adobe PPD specification.
