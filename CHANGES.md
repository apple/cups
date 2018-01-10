CHANGES - 2.3b2 - 2018-01-09
============================


Changes in CUPS v2.3b2
----------------------

- Localization changes (Issue #5210)
- Build fixes (Issue #5217)
- The `cupsGetDests` and `cupsEnumDests` functions no longer filter out local
  print services like IPP USB devices (Issue #5206)
- The `ppdInstallableConflict` tested too many constraints (Issue #5213)


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
