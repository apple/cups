---
title: Updated CUPS 1.1.9 Source Distribution Released
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

<P>An updated source distribution for CUPS 1.1.9 is now available.This patch corrects the following problems:</P><UL>	<LI>The configure script did not substitute the	correct user and group names.	<LI>The configure script did not use the full path	to the install-sh script when it was used.	<LI>The pstoraster filter did not correctly support	DuplexTumble mode for printers that used flip	duplexing.	<LI>The cups.list.in file was missing from the	distribution.	<LI>The New DeskJet series driver did not use the	correct OrderDependency for the Duplex option.	<LI>Use read() instead of fread() to read piped	print files in lpr/lp.  This avoids a bug in the	HP-UX 10.20 fread() function.</UL>
