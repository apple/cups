---
title: Common UNIX Printing System 1.3.9
layout: post
---

CUPS 1.3.9 is now available for download from:    http://www.cups.org/software.htmlIt contains the following fixes:
- SECURITY: The HP-GL/2 filter did not range check pen numbers (Issue #2911)
- SECURITY: The SGI image file reader did not range check 16-bit run lengths (Issue #2918)
- SECURITY: The text filter did not range check cpi, lpi, or column values (Issue #2919)
- Documentation updates (Issue #2904, Issue #2944)
- The French web admin page was never updated (Issue #2963)
- The IPP backend did not retry print jobs when the printer reported itself as busy or unavailable (Issue #2951)
- The &quot;Set Allowed Users&quot; web interface did not handle trailing whitespace correctly (Issue #2956)
- The PostScript filter did not work with Adobe applications using custom page sizes (Issue #2968)
- The Mac OS X USB backend did not work with some printers that reported a bad 1284 device ID.
- The scheduler incorrectly resolved the client connection address when HostNameLookups was set to Off (Issue #2946)
- The IPP backend incorrectly stopped the local queue if the remote server reported the &quot;paused&quot; state.
- The cupsGetDests() function did not catch all types of request errors.
- The scheduler did not always log &quot;job queued&quot; messages (Issue #2943)
- The scheduler did not support destination filtering using the printer-location attribute properly (Issue #2945)
- The scheduler did not send the server-started, server-restarted, or server-stopped events (Issue #2927)
- The scheduler no longer enforces configuration file permissions on symlinked files (Issue #2937)
- CUPS now reinitializes the DNS resolver on failures (Issue #2920)
- The CUPS desktop menu item was broken (Issue #2924)
- The PPD parser was too strict about missing keyword values in &quot;relaxed&quot; mode.
- The PostScript filter incorrectly mirrored landscape documents.
- The scheduler did not correctly update the auth-info-required value(s) if the AuthType was Default.
- The scheduler required Kerberos authentication for all operations on remote Kerberized printers instead of just for the operations that needed it.
- The socket backend could wait indefinitely for back- channel data with some devices.
- PJL panel messages were not reset correctly on older printers (Issue #2909)
- cupsfilter used the wrong default path (Issue #2908)
- Fixed address matching for &quot;BrowseAddress @IF(name)&quot; (Issue #2910)
- Fixed compiles on AIX.
- Firefox 3 did not work with the CUPS web interface in SSL mode (Issue #2892)
- Custom options with multiple parameters were not emitted correctly.
- Refined the cupstestppd utility.
- ppdEmit*() did not support custom JCL options (Issue #2889)
- The cupstestppd utility incorrectly reported missing &quot;en&quot; base translations (Issue #2887)
