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
- Fixed crash in rastertopwg (Apple issue #5773)
- Fixed cupsManualCopies values in IPP Everywhere PPDs (Apple issue #5807)
