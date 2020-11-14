Changes in OpenPrinting CUPS
============================

Changes in CUPS v2.3.3op1
-------------------------

- ippeveprinter now supports multiple icons and strings files.
- ippeveprinter now uses the system's FQDN with Avahi.
- ippeveprinter now supports Get-Printer-Attributes on "/".
- ippeveprinter now uses a deterministic "printer-uuid" value.
- ippeveprinter now uses system sounds on macOS for Identify-Printer.
- Updated ippfind to look for files in "~/Desktop" on Windows.
- Updated ippfind to honor `SKIP-XXX` directives with `PAUSE`.
- Updated IPP Everywhere support to work around printers that only advertise
  color raster support but really also support grayscale (Issue #1)
- ipptool now supports DNS-SD URIs like `ipps://My%20Printer._ipps._tcp.local`
  (Issue #5)
- The scheduler now allows root backends to have world read permissions but not
  world execute permissions (Issue #21)
- Failures to bind IPv6 listener sockets no longer cause errors if IPv6 is
  disabled on the host (Issue #25)
- The SNMP backend now supports the HP and Ricoh vendor MIBs (Issue #28)
- The scheduler no longer includes a timestamp in files it writes (Issue #29)
- The systemd service names are now "cups.service" and "cups-lpd.service"
  (Issue #30, Issue #31)
- The scheduler no longer adds the local hostname to the ServerAlias list
  (Issue #32)
- Added `LogFileGroup` directive in "cups-files.conf" to control the group
  owner of log files (Issue #34)
- Added `--with-max-log-size` configure option (Issue #35)
- Added `--enable-sync-on-close` configure option (Issue #37)
- Added `--with-error-policy` configure option (Issue #38)
- IPP Everywhere PPDs could have an "unknown" default InputSlot (Issue #44)
- The `httpAddrListen` function now uses a listen backlog of 128.
- Added USB quirks (Apple issue #5789, #5823, #5831)
- Fixed IPP Everywhere v1.1 conformance issues in ippeveprinter.
- Fixed DNS-SD name collision support in ippeveprinter.
- Fixed compiler and code analyzer warnings.
- Fixed TLS support on Windows.
- Fixed ippfind sub-type searches with Avahi.
- Fixed the default hostname used by ippeveprinter on macOS.
- Fixed resolution of local IPP-USB printers with Avahi.
- Fixed coverity issues (Issue #2)
- Fixed `httpAddrConnect` issues (Issue #3)
- Fixed web interface device URI issue (Issue #4)
- Fixed lp/lpr "printer/class not found" error reporting (Issue #6)
- Fixed xinetd support for LPD clients (Issue #7)
- Fixed libtool build issue (Issue #11)
- Fixed a memory leak in the scheduler (Issue #12)
- Fixed a potential integer overflow in the PPD hashing code (Issue #13)
- Fixed output-bin and print-quality handling issues (Issue #18)
- Fixed PPD options getting mapped to odd IPP values like "tray---4" (Issue #23)
- Fixed remote access to the cupsd.conf and log files (Issue #24)
- Fixed the automated test suite when running in certain build/CI environments
  (Issue #25)
- Fixed a logging regression caused by a previous change for Apple issue #5604
  (Issue #25)
- Fixed fax phone number handling with GNOME (Issue #40)
- Fixed potential rounding error in rastertopwg filter (Issue #41)
- Fixed the "uri-security-supported" value from the scheduler (Issue #42)
- Fixed IPP backend crash bug with "printer-alert" values (Issue #43)
- Fixed crash in rastertopwg (Apple issue #5773)
- Fixed cupsManualCopies values in IPP Everywhere PPDs (Apple issue #5807)
