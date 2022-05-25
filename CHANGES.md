Change History
==============

Changes in CUPS v2.3.6
----------------------
- CVE-2022-26691: An incorrect comparison in local admin authentication.


Changes in CUPS v2.3.5
----------------------

- The automated test suite can now be activated using `make test` for
  consistency with other projects and CI environments - the old `make check`
  continues to work as well, and the previous test server behavior can be
  accessed by running `make testserver`.
- ippeveprinter now supports multiple icons and strings files.
- ippeveprinter now uses the system's FQDN with Avahi.
- ippeveprinter now supports Get-Printer-Attributes on "/".
- ippeveprinter now uses a deterministic "printer-uuid" value.
- ippeveprinter now uses system sounds on macOS for Identify-Printer.
- Updated ippfind to look for files in "~/Desktop" on Windows.
- Updated ippfind to honor `SKIP-XXX` directives with `PAUSE`.
- Updated IPP Everywhere support to work around printers that only advertise
  color raster support but really also support grayscale (OpenPrinting #1)
- ipptool now supports DNS-SD URIs like `ipps://My%20Printer._ipps._tcp.local`
  (OpenPrinting #5)
- ipptool now supports monitoring the printer state while submitting a job
  with the `MONITOR-PRINTER-STATE` directive (OpenPrinting #153)
- ipptool now supports testing for unique values with the `WITH-DISTINCT-VALUES`
  predicate (OpenPrinting #153)
- ipptool now supports retrying requests on a `server-error-busy` status code
  (OpenPrinting #153)
- ipptool now supports `value-tag(MAX)` and `value-tag(MIN:MAX)` for the
  `OF-TYPE` predicate (OpenPrinting #153)
- The scheduler now allows root backends to have world read permissions but not
  world execute permissions (OpenPrinting #21)
- Failures to bind IPv6 listener sockets no longer cause errors if IPv6 is
  disabled on the host (OpenPrinting #25)
- The SNMP backend now supports the HP and Ricoh vendor MIBs (OpenPrinting #28)
- The scheduler no longer includes a timestamp in files it writes (OpenPrinting #29)
- IPP Everywhere PPDs could have an "unknown" default InputSlot (OpenPrinting #44)
- The `httpAddrListen` function now uses a listen backlog of 128.
- The PPD functions now treat boolean values as case-insensitive (OpenPrinting #106)
- Temporary queue names no longer end with an underscore (OpenPrinting #110)
- Added USB quirks (Issue #5789, #5766, #5823, #5831, #5838, #5843, #5867)
- Fixed IPP Everywhere v1.1 conformance issues in ippeveprinter.
- Fixed DNS-SD name collision support in ippeveprinter.
- Fixed compiler and code analyzer warnings.
- Fixed TLS support on Windows.
- Fixed ippfind sub-type searches with Avahi.
- Fixed the default hostname used by ippeveprinter on macOS.
- Fixed resolution of local IPP-USB printers with Avahi.
- Fixed coverity issues (OpenPrinting #2)
- Fixed `httpAddrConnect` issues (OpenPrinting #3)
- Fixed web interface device URI issue (OpenPrinting #4)
- Fixed lp/lpr "printer/class not found" error reporting (OpenPrinting #6)
- Fixed a memory leak in the scheduler (OpenPrinting #12)
- Fixed a potential integer overflow in the PPD hashing code (OpenPrinting #13)
- Fixed output-bin and print-quality handling issues (OpenPrinting #18)
- Fixed PPD options getting mapped to odd IPP values like "tray---4" (OpenPrinting #23)
- Fixed remote access to the cupsd.conf and log files (OpenPrinting #24)
- Fixed a logging regression caused by a previous change for Issue #5604
  (OpenPrinting #25)
- Fixed the "uri-security-supported" value from the scheduler (OpenPrinting #42)
- Fixed IPP backend crash bug with "printer-alert" values (OpenPrinting #43)
- Fixed default options that incorrectly use the "custom" prefix (OpenPrinting #48)
- Fixed a memory leak when resolving DNS-SD URIs (OpenPrinting #49)
- Fixed cupsManualCopies values in IPP Everywhere PPDs (Issue #5807)
- Fixed duplicate ColorModel entries for AirPrint printers (Issue 59)
- Fixed crash bug in `ppdOpen` (OpenPrinting #64, OpenPrinting #78)
- Fixed regression in `snprintf` emulation function (OpenPrinting #67)
- Fixed reporting of printer instances when enumerating and when no options are
  set for the main instance (OpenPrinting #71)
- Fixed segfault in help.cgi when searching in man pages (OpenPrinting #81)
- Fixed a bug in ipptool that caused the reuse of request IDs when repeating a
  test (OpenPrinting #153)
- Root certificates were incorrectly stored in "~/.cups/ssl".
- Fixed a PPD memory leak caused by emulator definitions (OpenPrinting #124)
- Fixed a `DISPLAY` bug in `ipptool` (OpenPrinting #139)
- `httpReconnect2` did not reset the socket file descriptor when the TLS
  negotiation failed (Issue #5907)
- `httpUpdate` did not reset the socket file descriptor when the TLS
  negotiation failed (Apple #5915)
- The `ippeveprinter` tool now automatically uses an available port.
- The IPP backend now retries Validate-Job requests (OpenPrinting #132)
- Removed support for the (long deprecated and unused) `KeepAliveTimeout`
  directive in `cupsd.conf` (Issue #5733)
- Fixed `@IF(name)` handling in `cupsd.conf` (Issue #5918)
- The scheduler now supports the "everywhere" model directly (Issue #5919)
- Fixed documentation and added examples for CUPS' limited CGI support
  (Issue #5940)
- Fixed the `lpc` command prompt (Issue #5946)
- Fixed `job-pages-per-set` value for duplex print jobs.


Changes in CUPS v2.3.4
----------------------

- CVE-2020-10001: Fixed a buffer (read) overflow in the `ippReadIO` function.


Changes in CUPS v2.3.3
----------------------

- CVE-2020-3898: The `ppdOpen` function did not handle invalid UI
  constraint.  `ppdcSource::get_resolution` function did not handle
  invalid resolution strings.
- CVE-2019-8842: The `ippReadIO` function may under-read an extension
  field.
- Fixed WARNING_OPTIONS support for GCC 9.x


Changes in CUPS v2.3.2
----------------------

- Localization updates.


Changes in CUPS v2.3.1
----------------------

- Documentation updates (Issue #5661, #5674, #5682)
- CVE-2019-2228: The `ippSetValuetag` function did not validate the default
  language value.
- Fixed a crash bug in the web interface (Issue #5621)
- The PPD cache code now looks up page sizes using their dimensions
  (Issue #5633)
- PPD files containing "custom" option keywords did not work (Issue #5639)
- Added a workaround for the scheduler's systemd support (Issue #5640)
- On Windows, TLS certificates generated on February 29 would likely fail
  (Issue #5643)
- Added a DigestOptions directive for the `client.conf` file to control whether
  MD5-based Digest authentication is allowed (Issue #5647)
- Fixed a bug in the handling of printer resource files (Issue #5652)
- The libusb-based USB backend now reports an error when the distribution
  permissions are wrong (Issue #5658)
- Added paint can labels to Dymo driver (Issue #5662)
- The `ippeveprinter` program now supports authentication (Issue #5665)
- The `ippeveprinter` program now advertises DNS-SD services on the correct
  interfaces, and provides a way to turn them off (Issue #5666)
- The `--with-dbusdir` option was ignored by the configure script (Issue #5671)
- Sandboxed applications were not able to get the default printer (Issue #5676)
- Log file access controls were not preserved by `cupsctl` (Issue #5677)
- Default printers set with `lpoptions` did not work in all cases (Issue #5681,
  Issue #5683, Issue #5684)
- Fixed an error in the jobs web interface template (Issue #5694)
- Fixed an off-by-one error in `ippEnumString` (Issue #5695)
- Fixed some new compiler warnings (Issue #5700)
- Fixed a few issues with the Apple Raster support (rdar://55301114)
- The IPP backend did not detect all cases where a job should be retried using
  a raster format (rdar://56021091)
- Fixed spelling of "fold-accordion".
- Fixed the default common name for TLS certificates used by `ippeveprinter`.
- Fixed the option names used for IPP Everywhere finishing options.
- Added support for the second roll of the DYMO Twin/DUO label printers.


Changes in CUPS v2.3.0
----------------------

- CVE-2019-8696 and CVE-2019-8675: Fixed SNMP buffer overflows (rdar://51685251)
- Added a GPL2/LGPL2 exception to the new CUPS license terms.
- Documentation updates (Issue #5604)
- Localization updates (Issue #5637)
- Fixed a bug in the scheduler job cleanup code (Issue #5588)
- Fixed builds when there is no TLS library (Issue #5590)
- Eliminated some new GCC compiler warnings (Issue #5591)
- Removed dead code from the scheduler (Issue #5593)
- "make" failed with GZIP options (Issue #5595)
- Fixed potential excess logging from the scheduler when removing job files
  (Issue #5597)
- Fixed a NULL pointer dereference bug in `httpGetSubField2` (Issue #5598)
- Added FIPS-140 workarounds for GNU TLS (Issue #5601, Issue #5622)
- The scheduler no longer provides a default value for the description
  (Issue #5603)
- The scheduler now logs jobs held for authentication using the error level so
  it is clear what happened (Issue #5604)
- The `lpadmin` command did not always update the PPD file for changes to the
  `cupsIPPSupplies` and `cupsSNMPSupplies` keywords (Issue #5610)
- The scheduler now uses both the group's membership list as well as the
  various OS-specific membership functions to determine whether a user belongs
  to a named group (Issue #5613)
- Added USB quirks rule for HP LaserJet 1015 (Issue #5617)
- Fixed some PPD parser issues (Issue #5623, Issue #5624)
- The IPP parser no longer allows invalid member attributes in collections
  (Issue #5630)
- The configure script now treats the "wheel" group as a potential system
  group (Issue #5638)
- Fixed a USB printing issue on macOS (rdar://31433931)
- Fixed IPP buffer overflow (rdar://50035411)
- Fixed memory disclosure issue in the scheduler (rdar://51373853)
- Fixed DoS issues in the scheduler (rdar://51373929)
- Fixed an issue with unsupported "sides" values in the IPP backend
  (rdar://51775322)
- The scheduler would restart continuously when idle and printers were not
  shared (rdar://52561199)
- Fixed an issue with `EXPECT !name WITH-VALUE ...` tests.
- Fixed a command ordering issue in the Zebra ZPL driver.
- Fixed a memory leak in `ppdOpen`.


Changes in CUPS v2.3rc1
-----------------------

- The `cups-config` script no longer adds extra libraries when linking against
  shared libraries (Issue #5261)
- The supplied example print documents have been optimized for size
  (Issue #5529)
- The `cupsctl` command now prevents setting "cups-files.conf" directives
  (Issue #5530)
- The "forbidden" message in the web interface is now explained (Issue #5547)
- The footer in the web interface covered some content on small displays
  (Issue #5574)
- The libusb-based USB backend now enforces read limits, improving print speed
  in many cases (Issue #5583)
- The `ippeveprinter` command now looks for print commands in the "command"
  subdirectory.
- The `ipptool` command now supports `$date-current` and `$date-start` variables
  to insert the current and starting date and time values, as well as ISO-8601
  relative time values such as "PT30S" for 30 seconds in the future.


Changes in CUPS v2.3b8
----------------------

- Media size matching now uses a tolerance of 0.5mm (rdar://33822024)
- The lpadmin command would hang with a bad PPD file (rdar://41495016)
- Fixed a potential crash bug in cups-driverd (rdar://46625579)
- Fixed a performance regression with large PPDs (rdar://47040759)
- Fixed a memory reallocation bug in HTTP header value expansion
  (rdar://problem/50000749)
- Timed out job submission now yields an error (Issue #5570)
- Restored minimal support for the `Emulators` keyword in PPD files to allow
  old Samsung printer drivers to continue to work (Issue #5562)
- The scheduler did not encode octetString values like "job-password" correctly
  for the print filters (Issue #5558)
- The `cupsCheckDestSupported` function did not check octetString values
  correctly (Issue #5557)
- Added support for `UserAgentTokens` directive in "client.conf" (Issue #5555)
- Updated the systemd service file for cupsd (Issue #5551)
- The `ippValidateAttribute` function did not catch all instances of invalid
  UTF-8 strings (Issue #5509)
- Fixed an issue with the self-signed certificates generated by GNU TLS
  (Issue #5506)
- Fixed a potential memory leak when reading at the end of a file (Issue #5473)
- Fixed potential unaligned accesses in the string pool (Issue #5474)
- Fixed a potential memory leak when loading a PPD file (Issue #5475)
- Added a USB quirks rule for the Lexmark E120n (Issue #5478)
- Updated the USB quirks rule for Zebra label printers (Issue #5395)
- Fixed a compile error on Linux (Issue #5483)
- The lpadmin command, web interface, and scheduler all queried an IPP
  Everywhere printer differently, resulting in different PPDs for the same
  printer (Issue #5484)
- The web interface no longer provides access to the log files (Issue #5513)
- Non-Kerberized printing to Windows via IPP was broken (Issue #5515)
- Eliminated use of private headers and some deprecated macOS APIs (Issue #5516)
- The scheduler no longer stops a printer if an error occurs when a job is
  canceled or aborted (Issue #5517)
- Added a USB quirks rule for the DYMO 450 Turbo (Issue #5521)
- Added a USB quirks rule for Xerox printers (Issue #5523)
- The scheduler's self-signed certificate did not include all of the alternate
  names for the server when using GNU TLS (Issue #5525)
- Fixed compiler warnings with newer versions of GCC (Issue #5532, Issue #5533)
- Fixed some PPD caching and IPP Everywhere PPD accounting/password bugs
  (Issue #5535)
- Fixed `PreserveJobHistory` bug with time values (Issue #5538)
- The scheduler no longer advertises the HTTP methods it supports (Issue #5540)
- Localization updates (Issue #5461, Issues #5471, Issue #5481, Issue #5486,
  Issue #5489, Issue #5491, Issue #5492, Issue #5493, Issue #5494, Issue #5495,
  Issue #5497, Issue #5499, Issue #5500, Issue #5501, Issue #5504)
- The scheduler did not always idle exit as quickly as it could.
- Added a new `ippeveprinter` command based on the old ippserver sample code.


Changes in CUPS v2.3b7
----------------------

- Fixed some build failures (Issue #5451, Issue #5463)
- Running ppdmerge with the same input and output filenames did not work as
  advertised (Issue #5455)


Changes in CUPS v2.3b6
----------------------

- Localization update (Issue #5339, Issue #5348, Issue #5362, Issue #5408,
  Issue #5410)
- Documentation updates (Issue #5369, Issue #5402, Issue #5403, Issue #5404)
- CVE-2018-4300: Linux session cookies used a predictable random number seed.
- All user commands now support the `--help` option (Issue #5326)
- The `lpoptions` command now works with IPP Everywhere printers that have not
  yet been added as local queues (Issue #5045)
- The lpadmin command would create a non-working printer in some error cases
  (Issue #5305)
- The scheduler would crash if an empty `AccessLog` directive was specified
  (Issue #5309)
- The scheduler did not idle-exit on some Linux distributions (Issue #5319)
- Fixed a regression in the changes to ippValidateAttribute (Issue #5322,
  Issue #5330)
- Fixed a crash bug in the Epson dot matrix driver (Issue #5323)
- Automatic debug logging of job errors did not work with systemd (Issue #5337)
- The web interface did not list the IPP Everywhere "driver" (Issue #5338)
- The scheduler did not report all of the supported job options and values
  (Issue #5340)
- The IPP Everywhere "driver" now properly supports face-up printers
  (Issue #5345)
- Fixed some typos in the label printer drivers (Issue #5350)
- Setting the `Community` name to the empty string in `snmp.conf` now disables
  SNMP supply level monitoring by all the standard network backends
  (Issue #5354)
- Multi-file jobs could get stuck if the backend failed (Issue #5359,
  Issue #5413)
- The IPP Everywhere "driver" no longer does local filtering when printing to
  a shared CUPS printer (Issue #5361)
- The lpadmin command now correctly reports IPP errors when configuring an
  IPP Everywhere printer (Issue #5370)
- Fixed some memory leaks discovered by Coverity (Issue #5375)
- The PPD compiler incorrectly terminated JCL options (Issue #5379)
- The cupstestppd utility did not generate errors for missing/mismatched
  CloseUI/JCLCloseUI keywords (Issue #5381)
- The scheduler now reports the actual location of the log file (Issue #5398)
- Added USB quirk rules (Issue #5395, Issue #5420, Issue #5443)
- The generated PPD files for IPP Everywhere printers did not contain the
  cupsManualCopies keyword (Issue #5433)
- Kerberos credentials might be truncated (Issue #5435)
- The handling of `MaxJobTime 0` did not match the documentation (Issue #5438)
- Fixed a bug adding a queue with the `-E` option (Issue #5440)
- The `cupsaddsmb` program has been removed (Issue #5449)
- The `cupstestdsc` program has been removed (Issue #5450)
- The scheduler was being backgrounded on macOS, causing applications to spin
  (rdar://40436080)
- The scheduler did not validate that required initial request attributes were
  in the operation group (rdar://41098178)
- Authentication in the web interface did not work on macOS (rdar://41444473)
- Fixed an issue with HTTP Digest authentication (rdar://41709086)
- The scheduler could crash when job history was purged (rdar://42198057)
- Fixed a crash bug when mapping PPD duplex options to IPP attributes
  (rdar://46183976)
- Fixed a memory leak for some IPP (extension) syntaxes.
- The `cupscgi`, `cupsmime`, and `cupsppdc` support libraries are no longer
  installed as shared libraries.
- The `snmp` backend is now deprecated.


Changes in CUPS v2.3b5
----------------------

- The `ipptool` program no longer checks for duplicate attributes when running
  in list or CSV mode (Issue #5278)
- The `cupsCreateJob`, `cupsPrintFile2`, and `cupsPrintFiles2` APIs did not use
  the supplied HTTP connection (Issue #5288)
- Fixed another crash in the scheduler when adding an IPP Everywhere printer
  (Issue #5290)
- Added a workaround for certain web browsers that do not support multiple
  authentication schemes in a single response header (Issue #5289)
- Fixed policy limits containing the `All` operation (Issue #5296)
- The scheduler was always restarted after idle-exit with systemd (Issue #5297)
- Added a USB quirks rule for the HP LaserJet P1102 (Issue #5310)
- The mailto notifier did not wait for the welcome message (Issue #5312)
- Fixed a parsing bug in the pstops filter (Issue #5321)
- Documentation updates (Issue #5299, Issue #5301, Issue #5306)
- Localization updates (Issue #5317)
- The scheduler allowed environment variables to be specified in the
  `cupsd.conf` file (rdar://37836779, rdar://37836995, rdar://37837252,
  rdar://37837581)
- Fax queues did not support pause (p) or wait-for-dialtone (w) characters
  (rdar://39212256)
- The scheduler did not validate notify-recipient-uri values properly
  (rdar://40068936)
- The IPP parser allowed invalid group tags (rdar://40442124)
- Fixed a parsing bug in the new authentication code.


Changes in CUPS v2.3b4
----------------------

- NOTICE: Printer drivers are now deprecated (Issue #5270)
- Kerberized printing to another CUPS server did not work correctly
  (Issue #5233)
- Fixed printing to some IPP Everywhere printers (Issue #5238)
- Fixed installation of filters (Issue #5247)
- The scheduler now supports using temporary print queues for older IPP/1.1
  print queues like those shared by CUPS 1.3 and earlier (Issue #5241)
- Star Micronics printers need the "unidir" USB quirk rule (Issue #5251)
- Documentation fixes (Issue #5252)
- Fixed a compile issue when PAM is not available (Issue #5253)
- Label printers supported by the rastertolabel driver don't support SNMP, so
  don't delay printing to test it (Issue #5256)
- The scheduler could crash while adding an IPP Everywhere printer (Issue #5258)
- The Lexmark Optra E310 printer needs the "no-reattach" USB quirk rule
  (Issue #5259)
- Systemd did not restart cupsd when configuration changes were made that
  required a restart (Issue #5263)
- The IPP Everywhere PPD generator did not include the `cupsJobPassword`
  keyword, when supported (Issue #5265)
- Fixed an Avahi crash bug in the scheduler (Issue #5268)
- Raw print queues are now deprecated (Issue #5269)
- Fixed an RPM packaging problem (Issue #5276)
- The IPP backend did not properly detect failed PDF prints (rdar://34055474)
- TLS connections now properly timeout (rdar://34938533)
- Temp files could not be created in some sandboxed applications
  (rdar://37789645)
- The ipptool `--ippserver` option did not encode out-of-band attributes
  correctly.
- Added public `cupsEncodeOption` API for encoding a single option as an IPP
  attribute.
- Removed support for the `-D_PPD_DEPRECATED=""` developer cheat - the PPD API
  should no longer be used.
- Removed support for `-D_IPP_PRIVATE_STRUCTURES=1` developer cheat - the IPP
  accessor functions should be used instead.


Changes in CUPS v2.3b3
----------------------

- More fixes for printing to old CUPS servers (Issue #5211)
- The IPP Everywhere PPD generator did not support deep grayscale or 8-bit per
  component AdobeRGB (Issue #5227)
- Additional changes for the scheduler to substitute default values for invalid
  job attributes when running in "relaxed conformance" mode (Issue #5229)
- Localization changes (Issue #5232, rdar://37068158)
- The `cupsCopyDestInfo` function did not work with all print queues
  (Issue #5235)


Changes in CUPS v2.3b2
----------------------

- Localization changes (Issue #5210)
- Build fixes (Issue #5217)
- IPP Everywhere PPDs were not localized to English (Issue #5205)
- The `cupsGetDests` and `cupsEnumDests` functions no longer filter out local
  print services like IPP USB devices (Issue #5206)
- The `cupsCopyDest` function now correctly copies the `is_default` value
  (Issue #5208)
- Printing to old CUPS servers has been fixed (Issue #5211)
- The `ppdInstallableConflict` tested too many constraints (Issue #5213)
- All HTTP field values can now be longer than `HTTP_MAX_VALUE` bytes
  (Issue #5216)
- Added a USB quirk rule for Canon MP280 series printers (Issue #5221)
- The `cupsRasterWritePixels` function did not correctly swap bytes for some
  formats (Issue #5225)
- Fixed an issue with mapping finishing options (rdar://34250727)
- The `ppdLocalizeIPPReason` function incorrectly returned a localized version
  of "none" (rdar://36566269)
- The scheduler did not add ".local" to the default DNS-SD host name when
  needed.


Changes in CUPS v2.3b1
----------------------

- CUPS is now provided under the Apache License, Version 2.0.
- Documentation updates (Issue #4580, Issue #5177, Issue #5192)
- The `cupsCopyDestConflicts` function now handles collection attribute
  ("media-col", "finishings-col", etc.) constraints (Issue #4096)
- The `lpoptions` command incorrectly saved default options (Issue #4717)
- The `lpstat` command now reports when new jobs are being held (Issue #4761)
- The `ippfind` command now supports finding printers whose name starts with an
  underscore (Issue #4833)
- The CUPS library now supports the latest HTTP Digest authentication
  specification including support for SHA-256 (Issue #4862)
- The scheduler now supports the "printer-id" attribute (Issue #4868)
- No longer support backslash, question mark, or quotes in printer names
  (Issue #4966)
- The scheduler no longer logs pages as they are printed, instead just logging
  a total of the pages printed at job completion (Issue #4991)
- Dropped RSS subscription management from the web interface (Issue #5012)
- Bonjour printer sharing now uses the DNS-SD hostname (or ServerName value if
  none is defined) when registering shared printers on the network (Issue #5071)
- The `ipptool` command now supports writing `ippserver` attributes files
  (Issue #5093)
- The `lp` and `lpr` commands now provide better error messages when the default
  printer cannot be found (Issue #5096)
- The `lpadmin` command now provides a better error message when an unsupported
  System V interface script is used (Issue #5111)
- The scheduler did not write out dirty configuration and state files if there
  were open client connections (Issue #5118)
- The `SSLOptions` directive now supports `MinTLS` and `MaxTLS` options to
  control the minimum and maximum TLS versions that will be allowed,
  respectively (Issue #5119)
- Dropped hard-coded CGI scripting language support (Issue #5124)
- The `cupsEnumDests` function did not include options from the lpoptions
  files (Issue #5144)
- Fixed the `ippserver` sample code when threading is disabled or unavailable
  (Issue #5154)
- Added label markup to checkbox and radio button controls in the web interface
  templates (Issue #5161)
- Fixed group validation on OpenBSD (Issue #5166)
- Improved IPP Everywhere media support, including a new
  `cupsAddDestMediaOptions` function (Issue #5167)
- IPP Everywhere PPDs now include localizations of printer-specific media types,
  when available (Issue #5168)
- The cups-driverd program incorrectly stopped scanning PPDs as soon as a loop
  was seen (Issue #5170)
- IPP Everywhere PPDs now support IPP job presets (Issue #5179)
- IPP Everywhere PPDs now support finishing templates (Issue #5180)
- Fixed a journald support bug in the scheduler (Issue #5181)
- Fixed PAM module detection and added support for the common PAM definitions
  (Issue #5185)
- The scheduler now substitutes default values for invalid job attributes when
  running in "relaxed conformance" mode (Issue #5186)
- The scheduler did not work with older versions of uClibc (Issue #5188)
- The scheduler now generates a strings file for localizing PPD options
  (Issue #5194)
