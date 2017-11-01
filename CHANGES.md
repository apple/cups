CHANGES - 2.3b1 - 2017-11-01
============================


Changes in CUPS v2.3b1
----------------------

- Documentation updates (Issue #4580)
- The `lpstat` command now reports when new jobs are being held (Issue #4761)
- The scheduler now supports the "printer-id" attribute (Issue #4868)
- No longer support backslash, question mark, or quotes in printer names
  (Issue #4966)
- Dropped RSS subscription management from the web interface (Issue #5012)
- Bonjour printer sharing now uses the DNS-SD hostname (or ServerName value if
  none is defined) when registering shared printers on the network (Issue #5071)
- The `ipptool` command now supports writing `ippserver` attributes files
  (Issue #5093)
- The `lp` and `lpr` commands now provide better error messages when the default
  printer cannot be found (Issue #5096)
- The `lpadmin` command now provides a better error message when an unsupported
  System V interface script is used (Issue #5111)
- Dropped hard-coded CGI scripting language support (Issue #5124)
- Fixed the `ippserver` sample code when threading is disabled or unavailable
  (Issue #5154)
