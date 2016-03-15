@echo off
::
::  "$Id: document-tests.bat 12249 2014-11-14 12:54:05Z msweet $"
::
:: IPP Everywhere Printer Self-Certification Manual 1.0: Section 7: Document Data Tests.
::
:: Copyright 2014 by The Printer Working Group.
::
:: This program may be copied and furnished to others, and derivative works
:: that comment on, or otherwise explain it or assist in its implementation may
:: be prepared, copied, published and distributed, in whole or in part, without
:: restriction of any kind, provided that the above copyright notice and this
:: paragraph are included on all such copies and derivative works.
::
:: The IEEE-ISTO and the Printer Working Group DISCLAIM ANY AND ALL WARRANTIES,
:: WHETHER EXPRESS OR IMPLIED INCLUDING (WITHOUT LIMITATION) ANY IMPLIED
:: WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
::
:: Usage:
::
::   document-tests.bat 'Printer Name'
::

ippfind "%1._ipp._tcp.local." -x ipptool -P "\"%1 Document Results.plist\"" -I "{}" document-tests.test ";"

::
:: End of "$Id: document-tests.bat 12249 2014-11-14 12:54:05Z msweet $".
::
