CHANGES - 2.3b8 - 2018-12-14
============================


Changes in CUPS v2.3b8
----------------------

- Fixed a potential crash bug in cups-driverd (rdar://46625579)


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
- CVE-2018-4700: Linux session cookies used a predictable random number seed.
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
