---
title: Netgear WGPS606 Printer Configuration Mini HowTo
layout: post
permalink: /blog/:year-:month-:day-:title.html
---

I had a terrible time getting my HP1200 configured on the Netgear WGPS606 as a linux only user.  These are the simple steps on how to go about configuring it.
- Follow the manual's instructions for setting up the print server on the network via the web interface (server defaults: ip "192.168.0.102", netmask "255.255.255.0", uname "admin", password "password").Note: You are advised to set a static IP address, otherwise the print server may receive a different address after a power outage.
- Before proceeding, check your print server's firmware version.  You can find this on the status page of the web interface.  If the version is V1.0_020 or later, then proceed with setup.  If it is not, then go to http://kbserver.netgear.com/release_notes/d102696.asp and follow the directions to upgrade the firmware to V1.0_020 (latest as of August 6, 2005). 
- Now that it's on the network and has firmware V1.0_020 or later:  
- Unplug its power.
- Plug your printer into it.
- Turn on your printer.
- Plug it back in.
- Now, in a graphical browser.  
- Go to http://localhost:631
- Click "Administration" in the header.
- Authenticate with user: root, password: root's password
- Click "Add Printer" in the Printers section.
- Fill in at least "Name" and click "Continue"
- In the Device Dropdown box, select "LPD/LPR Host or Printer" and click "Continue"
- In the Device URI box append "://<your_wgps606_ip>/L1" or "://<your_wgps606_ip>/L2".  L1 is for the first printer, L2 is for the second.  In my case the full URI is lpd://192.168.100.104/L1.
- Select your printer's make and click "Continue"
- Select your printer's model and click "Continue"
- And your done, go ahead and send a test page.*NOTE: You can still set up the printer with pre V1.0_020 firmware, but you will have to do "Start Printer" after every job, and expect PCL errors, half completed jobs or jobs that spool but never print.***NOTE 2: Unfortunatly, printing multiple copies doesn't work.  To get around this problem, print to a file and then use the following script...**Script:

 #!/bin/bash
 
 function is_int() {
   [ "$1" -eq "$1" ] > /dev/null 2>&1
   return $?
 }
 
 i=0
 if ! is_int $1 || [ -z $2 ]; then
   echo "$0: USAGE: $0 <NUM_COPIES> <FILE_TO_PRINT>"
   exit 1
 fi
 
 if [ ! -e $2 ]; then
   echo "$0: $2: File does not exist."
   exit 2
 fi
 
 while (( $i < $1 )); do
   lp $2
   let i=$i+1
 done

Below added December, 20, 2006*NOTE 3: Mac OSX Tiger (10.4) ships with CUPS 1.1.  This device can be used through the Print & Fax pane under your System Prefences. (See Below)**Mac OSX Tiger configuration:
- Follow steps 1-3 above.
- Click "System Preferences" in the Apple menu (apple shaped icon).
- If you can't see all the possible preferences categories, click the "Show All" button under the title bar.
- In the "Hardware" group, click "Print & Fax."
- Under the printer list, there are two(2) buttons "+" and "-", click "+."
- In the window that pops up ("Printer Browser"), select "IP Printer."
- For "Protocol," select "Line Printer Daemon - LPD."
- Type the IP Address of the print server into the field marked "Address."
- Type the queue into the field marked "Queue."  This will be either "L1" or "L2."
- Give the printer a name in the "Name" field.
- Select the appropriate print driver in the "Print Using" drop down.
- Finally, click the "Add" button and you're done.
