---
title: PHP CUPS support (PHP5 class) V0.75
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

After several years from v0.74, The V.075 version of PhpPrintIPP is out.Homepage: http://www.nongnu.org/phpprintipp/PrintIPP is a PHP5 class which implements an IPP client (Internet Printing Protocol) on the web-server side.PrintIPP, in it's current state, do all RFC2911 operations,as well as some CUPS specific ones. PrintIPP is distributed under GNU LGPL. Thus, it is a Free Software.
- on-line documentation: http://www.nongnu.org/phpprintipp/usage (don't be afraid :P)
- anonymous CVS access: cvs -z3 -d:pserver:anonymous@cvs.savannah.nongnu.org:/sources/phpprintipp co phpprintipp
- latest public release: http://download.savannah.gnu.org/releases/phpprintipp/phpprintipp-CURRENT.tar.gz
- signature: http://download.savannah.gnu.org/releases/phpprintipp/phpprintipp-CURRENT.tar.gz.sig
- GPG key: http://savannah.gnu.org/users/harding
- Minimal example:   &lt;?php   require_once(PrintIPP.php);   $ipp = new PrintIPP();   $ipp->setHost("localhost");   $ipp->setPrinterURI("/printers/epson");   $ipp->setData("./testfiles/test-utf8.txt"); // Path to file or string.   $ipp->printJob();   // trees saving   $job = $ipp->last_job; // getting job uri   $ipp->cancelJob($job); // cancelling job ?&gt;
