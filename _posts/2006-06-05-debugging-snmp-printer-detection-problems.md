---
title: Debugging SNMP Printer Detection Problems
layout: post
---

The new SNMP network printer detection functionality in CUPS 1.2 sometimes exposes problems in vendor SNMP or IPP implementations. If you are experiencing long delays in loading the CUPS web interface administration page, or if you don't see your printer listed, the following instructions will help you to diagnose those problems and/or provide important feedback to the CUPS developers so that we can correct problems and improve the SNMP backend in future releases.

<H2>Quick Fixes</H2>

If you don't use "public" as your community name, create a text file called <VAR>/etc/cups/snmp.conf</VAR> and put the following line in it:

<PRE CLASS="command">
Community <I>your community name</I>
</PRE>

If you have more than one community name, list them all on separate lines.

If you don't support SNMP v1 on your network, you are currently "out of luck". That said, we will be adding v2, v2c, and v3 support in future CUPS releases once we have a handle on the actual requirements people have for such things. Please file or update an SNMP enhancement request (http://www.cups.org/str.php) with <em>specific</em> requirements you have - what you need supported, why you need it supported, and how you would like to see the functionality provided/exposed - so that we can do it "right" the first time.

<H2>Basic Debugging</H2>

The SNMP backend supports a debugging mode that is activated by running it from a shell prompt. If you are using Bash (/bin/bash), Bourne shell (/bin/sh), Korn shell (/bin/ksh), or Z shell (/bin/zsh), you can run the following command to get a verbose log of the SNMP backend:

<PRE CLASS="command">
CUPS_DEBUG_LEVEL=2 /usr/lib/cups/backend/snmp 2>&amp;1 | tee snmp.log
</PRE>

For C shell (/bin/csh) and TCsh (/bin/tcsh), use the following command instead:

<PRE CLASS="command">
(setenv CUPS_DEBUG_LEVEL 2; /usr/lib/cups/backend/snmp) |& tee snmp.log
</PRE>

On MacOS X you'll find the SNMP backend in /usr/libexec/cups/backend instead:

<PRE CLASS="command">
CUPS_DEBUG_LEVEL=2 /usr/libexec/cups/backend/snmp 2>&amp;1 | tee snmp.log
</PRE>

The output will look something like this:

<PRE STYLE="margin-left: 36pt">
 1  INFO: Using default SNMP Address @LOCAL
 2  INFO: Using default SNMP Community public
 3  DEBUG: Scanning for devices in "public" via "@LOCAL"...
 4  DEBUG: 0.000 Sending 46 bytes to 192.168.2.255...
 5  DEBUG: SEQUENCE 44 bytes
 6  DEBUG:     INTEGER 1 bytes 0
 7  DEBUG:     OCTET STRING 6 bytes "public"
 8  DEBUG:     Get-Request-PDU 31 bytes
 9  DEBUG:         INTEGER 4 bytes 1149539174
10  DEBUG:         INTEGER 1 bytes 0
11  DEBUG:         INTEGER 1 bytes 0
12  DEBUG:         SEQUENCE 17 bytes
13  DEBUG:             SEQUENCE 15 bytes
14  DEBUG:                 OID 11 bytes .1.3.6.1.2.1.25.3.2.1.2.1
15  DEBUG:                 NULL VALUE 0 bytes
16  DEBUG: 0.001 Received 55 bytes from 192.168.2.229...
17  DEBUG: community="public"
18  DEBUG: request-id=1149539174
19  DEBUG: error-status=0
20  DEBUG: SEQUENCE 53 bytes
21  DEBUG:     INTEGER 1 bytes 0
22  DEBUG:     OCTET STRING 6 bytes "public"
23  DEBUG:     Get-Response-PDU 40 bytes
24  DEBUG:         INTEGER 4 bytes 1149539174
25  DEBUG:         INTEGER 1 bytes 0
26  DEBUG:         INTEGER 1 bytes 0
27  DEBUG:         SEQUENCE 26 bytes
28  DEBUG:             SEQUENCE 24 bytes
29  DEBUG:                 OID 11 bytes .1.3.6.1.2.1.25.3.2.1.2.1
30  DEBUG:                 OID 9 bytes .1.3.6.1.2.1.25.3.1.5
31  DEBUG: add_cache(addr=0xbfffe170, addrname="192.168.2.229",
    uri="(null)", id="(null)", make_and_model="(null)")
32  DEBUG: 0.002 Sending 46 bytes to 192.168.2.229...
33  DEBUG: SEQUENCE 44 bytes
34  DEBUG:     INTEGER 1 bytes 0
35  DEBUG:     OCTET STRING 6 bytes "public"
36  DEBUG:     Get-Request-PDU 31 bytes
37  DEBUG:         INTEGER 4 bytes 1149539175
38  DEBUG:         INTEGER 1 bytes 0
39  DEBUG:         INTEGER 1 bytes 0
40  DEBUG:         SEQUENCE 17 bytes
41  DEBUG:             SEQUENCE 15 bytes
42  DEBUG:                 OID 11 bytes .1.3.6.1.2.1.25.3.2.1.3.1
43  DEBUG:                 NULL VALUE 0 bytes
44  DEBUG: 0.003 Received 69 bytes from 192.168.2.229...
45  DEBUG: community="public"
46  DEBUG: request-id=1149539175
47  DEBUG: error-status=0
48  DEBUG: SEQUENCE 67 bytes
49  DEBUG:     INTEGER 1 bytes 0
50  DEBUG:     OCTET STRING 6 bytes "public"
51  DEBUG:     Get-Response-PDU 54 bytes
52  DEBUG:         INTEGER 4 bytes 1149539175
53  DEBUG:         INTEGER 1 bytes 0
54  DEBUG:         INTEGER 1 bytes 0
55  DEBUG:         SEQUENCE 40 bytes
56  DEBUG:             SEQUENCE 38 bytes
57  DEBUG:                 OID 11 bytes .1.3.6.1.2.1.25.3.2.1.3.1
58  DEBUG:                 OCTET STRING 23 bytes "HP LaserJet 4000
    Series"
59  DEBUG: 1.001 Probing 192.168.2.229...
60  DEBUG: 1.001 Trying socket://192.168.2.229:9100...
61  DEBUG: 192.168.2.229 supports AppSocket!
62  DEBUG: 1.002 Scan complete!
63  network socket://192.168.2.229 "HP LaserJet 4000 Series"
    "HP LaserJet 4000 Series 192.168.2.229" ""
</PRE>

<H3>Dissecting the Output</H3>

The first two lines are just informational and let you know that the default community name and address are being used. Lines 3-15 contain the initial SNMP query for the device type OID (.1.3.6.1.2.1.25.3.2.1.2.1) from the Host MIB.

Lines 16-31 show the response we got from an HP LaserJet 4000 network printer. At this point we discover that it is a printer device and then send another SNMP query (lines 32-43) for the device description OID (.1.3.6.1.2.1.25.3.2.1.3.1) from the Host MIB as well.

Lines 44-58 show the response to the device description query, which tells us that this is an HP LaserJet 4000 Series printer.

On line 59 we start our active connection probe and discover that this print server supports the AppSocket (JetDirect) protocol on port 9100. 

Finally, line 63 shows the device information line for the print server that is sent to CUPS.

<H2>Reporting Problems</H2>

If you don't see your printer listed, or the wrong information is listed, then you need to gather more information on the printer. The easiest way to do this is to run the snmpwalk command:

<PRE CLASS="command">
snmpwalk -Cc -v 1 -c public <I>ip-address</I> | tee snmpwalk.log
</PRE>

where "ip-address" is the IP address of the printer or print server. You should see a *lot* of values stream by - the ones you want to see are:

<PRE STYLE="margin-left: 36pt">
HOST-RESOURCES-MIB::hrDeviceType.1 = OID: HOST-RESOURCES-TYPES::hrDevicePrinter
HOST-RESOURCES-MIB::hrDeviceDescr.1 = STRING: HP LaserJet 4000 Series
</PRE>

The hrDeviceType line should show hrDevicePrinter; if not, then your printer or print server doesn't identify itself as a printer. The hrDeviceDescr line should provide a human-readable string for the make and model of the printer, although in some cases you'll just see something less useful like "Axis OfficeBASIC Parallel Print Server".

Once you have collected the snmpwalk output, you should go to the CUPS Bugs &amp; Features page (http://www.cups.org/str.php) to submit a feature request to support your printer or print server. Be sure to attach those two log files you created - they will help us to identify the SNMP values we need to look for.

