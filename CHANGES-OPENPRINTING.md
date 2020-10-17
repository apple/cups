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
