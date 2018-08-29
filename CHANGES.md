CHANGES - 2.2.9 - 2018-08-28
============================


Changes in CUPS v2.2.9
----------------------

- Localization changes (Issue #5348, Issue #5362)
- Documentation updates (Issue #5369)
- The lpadmin command would create a non-working printer in some error cases
  (Issue #5305)
- The scheduler would crash if an empty `AccessLog` directive was specified
  (Issue #5309)
- Fixed a regression in the changes to ippValidateAttribute (Issue #5322,
  Issue #5330)
- Fixed a crash bug in the Epson dot matrix driver (Issue #5323)
- Automatic debug logging of job errors did not work with systemd (Issue #5337)
- The web interface did not list the IPP Everywhere "driver" (Issue #5338)
- Fixed some typos in the label printer drivers (Issue #5350)
- The IPP Everywhere "driver" no longer does local filtering when printing to
  a shared CUPS printer (Issue #5361)
- Fixed some memory leaks discovered by Coverity (Issue #5375)
- The PPD compiler incorrectly terminated JCL options (Issue #5379)
- The cupstestppd utility did not generate errors for missing/mismatched
  CloseUI/JCLCloseUI keywords (Issue #5381)
- The scheduler was being backgrounded on macOS, causing applications to spin
  (rdar://40436080)
- The scheduler did not validate that required initial request attributes were
  in the operation group (rdar://41098178)
- Authentication in the web interface did not work on macOS (rdar://41444473)
- Fixed an issue with HTTP Digest authentication (rdar://41709086)
- The scheduler could crash when job history was purged (rdar://42198057)
- Dropped non-working RSS subscriptions UI from web interface templates.
- Fixed a memory leak for some IPP (extension) syntaxes.


Changes in CUPS v2.2.8
----------------------

- Additional changes for the scheduler to substitute default values for invalid
  job attributes when running in "relaxed conformance" mode (Issue #5229)
- The `ipptool` program no longer checks for duplicate attributes when running
  in list or CSV mode (Issue #5278)
- Fixed builds without PAM (Issue #5283)
- Fixed `lpoptions` man page (Issue #5286)
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


Changes in CUPS v2.2.7
----------------------

- NOTICE: Raw print queues are now deprecated (Issue #5269)
- Fixed an Avahi crash bug in the scheduler (Issue #5268)
- The IPP Everywhere PPD generator did not include the `cupsJobPassword`
  keyword, when supported (Issue #5265)
- Systemd did not restart cupsd when configuration changes were made that
  required a restart (Issue #5263)
- The Lexmark Optra E310 printer needs the "no-reattach" USB quirk rule
  (Issue #5259)
- The scheduler could crash while adding an IPP Everywhere printer (Issue #5258)
- Label printers supported by the rastertolabel driver don't support SNMP, so
  don't delay printing to test it (Issue #5256)
- Fixed a compile issue when PAM is not available (Issue #5253)
- Documentation fixes (Issue #5252)
- Star Micronics printers need the "unidir" USB quirk rule (Issue #5251)
- The scheduler now supports using temporary print queues for older IPP/1.1
  print queues like those shared by CUPS 1.3 and earlier (Issue #5241)
- Fixed printing to some IPP Everywhere printers (Issue #5238)
- Kerberized printing to another CUPS server did not work correctly
  (Issue #5233)
- The `cupsRasterWritePixels` function did not correctly swap bytes for some
  formats (Issue #5225)
- Added a USB quirk rule for Canon MP280 series printers (Issue #5221)
- The `ppdInstallableConflict` tested too many constraints (Issue #5213)
- More fixes for printing to old CUPS servers (Issue #5211)
- The `cupsCopyDest` function now correctly copies the `is_default` value
  (Issue #5208)
- The scheduler did not work with older versions of uClibc (Issue #5188)
- The scheduler now substitutes default values for invalid job attributes when
  running in "relaxed conformance" mode (Issue #5186)
- Fixed PAM module detection and added support for the common PAM definitions
  (Issue #5185)
- Fixed a journald support bug in the scheduler (Issue #5181)
- The cups-driverd program incorrectly stopped scanning PPDs as soon as a loop
  was seen (Issue #5170)
- Fixed group validation on OpenBSD (Issue #5166)
- Fixed the `ippserver` sample code when threading is disabled or unavailable
  (Issue #5154)
- The `cupsEnumDests` function did not include options from the lpoptions files
  (Issue #5144)
- The `SSLOptions` directive now supports `MinTLS` and `MaxTLS` options to
  control the minimum and maximum TLS versions that will be allowed,
  respectively (Issue #5119)
- The scheduler did not write out dirty configuration and state files if there
  were open client connections (Issue #5118)
- The `lpadmin` command now provides a better error message when an unsupported
  System V interface script is used (Issue #5111)
- The `lp` and `lpr` commands now provide better error messages when the default
  printer cannot be found (Issue #5096)
- No longer support backslash, question mark, or quotes in printer names
  (Issue #4966)
- The CUPS library now supports the latest HTTP Digest authentication
  specification including support for SHA-256 (Issue #4862)
- The `lpstat` command now reports when new jobs are being held (Issue #4761)
- The `lpoptions` command incorrectly saved default options (Issue #4717)
- The `ppdLocalizeIPPReason` function incorrectly returned a localized version
  of "none" (rdar://36566269)
- TLS connections now properly timeout (rdar://34938533)
- The IPP backend did not properly detect failed PDF prints (rdar://34055474)
- Temporary files are now placed in the correct directory for sandboxed
  applications on macOS (rdar://problem/37789645)


Changes in CUPS v2.2.6
----------------------

- DBUS notifications could crash the scheduler (Issue #5143)
- Added USB quirks rules for Canon MP540 and Samsung ML-2160 (Issue #5148)
- Fixed TLS cipher suite selection with GNU TLS (Issue #5145, Issue #5150)
- Localization updates (Issue #5152)


Changes in CUPS v2.2.5
----------------------

- The scheduler's `-t` option did not force all errors to the standard error
  file, making debugging of configuration problems hard (Issue #5041)
- Fixed a typo in the CUPS Programming Manual (Issue #5042)
- Fixed RPM packaging issue (Issue #5043, Issue #5044)
- The `cupsGetDests` function incorrectly returned an empty list of printers if
  there was no default printer (Issue #5046)
- The `cupsGetDests` function waited too long for network printers (Issue #5049)
- Libtool support was completely broken with current libtool versions that use
  an incompatible command-line syntax (Issue #5050)
- Fixed a build issue with `--enable-mallinfo` (Issue #5051)
- The ippserver test program contained a deadlock issue (Issue #5054)
- The `cupsLocalizeDest*` functions did not provide base localizations for
  all registered IPP attributes and values (Issue #5056)
- The --enable-libtool configure option requires a path to the libtool program,
  but doesn't document or check for it (Issue #5062)
- Fixed the `SSLOptions DenyCBC` option when using GNU TLS (Issue #5065)
- Fixed the `ServerTokens None` option (Issue #5065)
- Fixed the default `ServerAlias` value from `ServerName` (Issue #5072)
- Fixed the adminurl field in the TXT record for fully-qualified `ServerName`
  values (Issue #5074)
- The scheduler now creates a PID file when not running on demand with a modern
  service launcher (Issue #5080)
- The web interface did not support newer language identifiers used by Microsoft
  web browsers (Issue #5803)
- Updated the cups-files.conf and cupsd.conf file documentation for missing
  directives (Issue #5084)
- Fixed an Avahi-related crash bug in the scheduler (Issue #5085, Issue #5086)
- Fixed the interactions between the "print-quality" and "cupsPrintQuality"
  options (Issue #5090)
- The IPP Everywhere PPD generator now sorts the supported resolutions before
  choosing them for draft, normal, and best quality modes (Issue #5091)
- Fixed the localization unit test on Linux (Issue #5097)
- The CUPS library did not reuse domain sockets (Issue #5098)
- Fixed the "make check" target for some environments (Issue #5099)
- The scheduler woke up once per second to remove old temporary queues
  (Issue #5100)
- Added USB quirk rule for Kyocera printer (Issue #5102, Issue #5103)
- Re-documented the limits of `file:///...` device URIs and moved the FileDevice
  directive in `cups-files.conf` to the list of deprecated configuration
  directives (Issue #5117)
- Added USB quirk rule for HP LaserJet 1160 printer (Issue #5121)
- Fixed the script interpreter detection in the configure script (Issue #5122)
- The network backends now retry on more error conditions (Issue #5123)
- Added a French translation of the web interface (Issue #5134)
- `cupsGetDests2` was not using the supplied HTTP connection (Issue #5135)
- `httpAddrConnect` leaked sockets in certain circumstances, causing some
  printers to hang (rdar://31965686)
- Fixed an issue with Chinese localizations on macOS (rdar://32419311)
- The IPP backend now always sends the "finishings" attribute for printers that
  support it because otherwise the client cannot override printer defaults
  (rdar://33169732)
- The `cupsGetNamedDest` function did not use the local default printer
  (rdar://33228500)
- The IPP backend incorrectly sent the "job-pages-per-set" attribute to PDF
  printers (rdar://33250434)
- Fixed the `cups.strings` file that is used on macOS (rdar://33287650)
- CUPS now sends the `Date` HTTP header in IPP requests (rdar://33302034)
- The `ippCopyAttribute` function did not copy out-of-band values correctly
  (rdar://33688003)
- Fixed the localization fallback code on macOS (rdar://33583699)
- The scheduler did not run with a high enough priority, causing problems on
  busy systems (rdar://33789342)
- Added support for Japanese Kaku 1 envelope size (rdar://34774110)
- The `ipptool` program's `-P` option did not work correctly.
- The `ipptool` program did not compare URI scheme or hostname components
  correctly for the WITH-ALL-HOSTNAMES, WITH-ALL-SCHEMES, WITH-HOSTNAME, or
  WITH-SCHEME predicates.


Changes in CUPS v2.2.4
----------------------

- The scheduler did not remove old job files (Issue #4987)
- cupsEnumDests did not return early when all printers had been discovered
  (Issue #4989)
- The CUPS build system now supports cross-compilation (Issue #4897)
- Added a new CUPS Programming Manual to replace the aging API documentation.
- Added the `cupsAddIntegerOption` and `cupsGetIntegerOption` functions
  (Issue #4992)
- The `cupsGetDests` and `cupsCreateJob` functions now support Bonjour printers
  (Issue #4993)
- Added a USB quirk rule for Lexmark E260dn printers (Issue #4994)
- Fixed a potential buffer overflow in the `cupstestppd` utility (Issue #4996)
- IPP Everywhere improvements (Issue #4998)
- Fixed the "cancel all jobs" function in the web interface for several
  languages (Issue #4999)
- Fixed issues with local queues (Issue #5003, Issue #5008, Issue #5009)
- The `lpstat` command now supports a `-e` option to enumerate local printers
  (either previously added or on the network) that can be accessed
  (Issue #5005)
- The `lp` and `lpr` commands now support printing to network printers that
  haven't yet been added (Issue #5006)
- Fixed a typo in the mime.types file.
- Fixed a bug in the Spanish web interface template (Issue #5016)
- The `cupsEnumDests*` and `cupsGetDest*` functions now report the value of the
  "printer-is-temporary" Printer Status attribute (Issue #5028)
- Added Chinese localization (Issue #5029)
- The `cupsCheckDestSupported` function did not support `NULL` values
  (Issue #5031)
- Fixed some issues in the RPM spec file (Issue #5032)
- The `cupsConnectDest` function now supports the `CUPS_DEST_FLAGS_DEVICE` flag
  for explicitly connecting to the device (printer) associated with the
  destination.
- The `SSLOptions` directive in "client.conf" and "cupsd.conf" now supports
  `DenyCBC` and `DenyTLS1.0` options (Issue #5037)


Changes in CUPS v2.2.3
----------------------

- The IPP backend could get into an infinite loop for certain errors, causing a
  hung queue (<rdar://problem/28008717>)
- The scheduler could pause responding to client requests in order to save state
  changes to disk (<rdar://problem/28690656>)
- Added support for PPD finishing keywords (Issue #4960, Issue #4961,
  Issue #4962)
- The IPP backend did not send a media-col attribute for just the source or type
  (Issue #4963)
- IPP Everywhere print queues did not always support all print qualities
  supported by the printer (Issue #4953)
- IPP Everywhere print queues did not always support all media types supported
  by the printer (Issue #4953)
- The IPP Everywhere PPD generator did not return useful error messages
  (Issue #4954)
- The IPP Everywhere finishings support did not work correctly with common UI or
  command-line options (Issue #4976)
- Fixed an error handling issue for the network backends (Issue #4979)
- The default cupsd.conf file did not work on systems compiled without Kerberos
  support (Issue #4947)
- The "reprint job" option was not available for some canceled jobs
  (Issue #4915)
- Updated the job listing in the web interface (Issue #4978)
- Fixed some localization issues on macOS (<rdar://problem/27245567>)


Changes in CUPS v2.2.2
----------------------

- Fixed some issues with the Zebra ZPL printer driver (Issue #4898)
- Fixed some issues with IPP Everywhere printer support (Issue #4893,
  Issue #4909, Issue #4916, Issue #4921, Issue #4923, Issue #4932, Issue #4933,
  Issue #4938)
- The rastertopwg filter could crash with certain input (Issue #4942)
- Optimized connection usage in the IPP backend (<rdar://problem/29547323>)
- The scheduler did not detect when an encrypted connection was closed by the
  client on Linux (Issue #4901)
- The cups-lpd program did not catch all legacy usage of ISO-8859-1
  (Issue #4899)
- Fixed builds on systems without a working poll() implementation (Issue #4905)
- Added a USB quirk rule for the Kyocera Ecosys P6026cdn (Issue #4900)
- The scheduler no longer creates log files on startup
  (<rdar://problem/28332470>)
- The ippContainsString function now uses case-insensitive comparisons for
  mimeMediaType, name, and text values in conformance with RFC 2911.
- The network backends now log the addresses that were found for a printer
  (<rdar://problem/29268474>)
- Let's Encrypt certificates did not work when the hostname contained uppercase
  letters (Issue #4919)
- Fixed reporting of printed pages in the web interface (Issue #4924)
- Updated systemd config files (Issue #4935)
- Updated documentation (PR #4896)
- Updated localizations (PR #4894, PR #4895, PR #4904, PR #4908, Issue #4946)
- Updated packaging files (Issue #4940)


Changes in CUPS v2.2.1
----------------------

- Added "CreateSelfSignedCerts" directive for cups-files.conf to control whether
  the scheduler automatically creates its own self-signed X.509 certificates for
  TLS connections (Issue #4876)
- http*Connect did not handle partial failures (Issue #4870)
- Addressed some build warnings on Linux (Issue #4881)
- cupsHashData did not use the correct hashing algorithm
  (<rdar://problem/28209220>)
- Updated man pages (PR #4885)
- Updated localizations (PR #4877, PR #4886)


Changes in CUPS v2.2.0
----------------------

- Normalized the TLS certificate validation code and added additional error
  messages to aid troubleshooting.
- The httpConnect functions did not work on Linux when cupsd was not running
  (Issue #4870)
- The --no-remote-any option of cupsctl had no effect (Issue #4866)
- http*Connect did not return early when all addresses failed (Issue #4870)


Changes in CUPS v2.2rc1
-----------------------

- Updated the list of supported IPP Everywhere media types.
- The IPP backend did not validate TLS credentials properly.
- The printer-state-message attribute was not cleared after a print job with no
  errors (Issue #4851)
- The CUPS-Add-Modify-Class and CUPS-Add-Modify-Printer operations did not
  always return an error for failed adds (Issue #4854)
- PPD files with names longer than 127 bytes did not work (Issue #4860)
- Updated localizations (Issue #4846, PR #4858)


Changes in CUPS v2.2b2
----------------------

- Added Upstart support (PR #4825)
- CUPS now supports Let's Encrypt certificates on Linux.


Changes in CUPS v2.2b1
----------------------

- All CUPS commands now support POSIX options (Issue #4813)
- The scheduler now restarts faster (Issue #4760)
- Improved performance of web interface with large numbers of jobs (Issue #3819)
- Encrypted printing can now be limited to only trusted printers and servers
  (<rdar://problem/25711658>)
- The scheduler now advertises PWG Raster attributes for IPP Everywhere clients
  (Issue #4428)
- The scheduler now logs informational messages for jobs at LogLevel "info"
  (Issue #4815)
- The scheduler now uses the getgrouplist function when available (Issue #4611)
- The IPP backend no longer enables compression by default except for certain
  raster formats that generally benefit from it (<rdar://problem/25166952>)
- The scheduler did not handle out-of-disk situations gracefully (Issue #4742)
- The LPD mini-daemon now detects invalid UTF-8 sequences in job, document, and
  user names (Issue #4748)
- The IPP backend now continues on to the next job when the remote server/
  printer puts the job on hold (<rdar://problem/24858548>)
- The scheduler did not cancel multi-document jobs immediately
  (<rdar://problem/24854834>)
- The scheduler did not return non-shared printers to local clients unless they
  connected to the domain socket (<rdar://problem/24566996>)
- The scheduler now reads the spool directory if one or more job cache entries
  point to deleted jobs (<rdar://problem/24048846>)
- Added support for disc media sizes (<rdar://problem/20219536>)
- The httpAddrConnect and httpConnect* APIs now try connecting to multiple
  addresses in parallel (<rdar://problem/20643153>)
- The cupsd domain socket is no longer world-accessible on macOS
  (<rdar://problem/7542560>)
- Interface scripts are no longer supported for security reasons
  (<rdar://problem/23135640>)
- Added a new cupsHashData API and support for hashed job passwords
  (<rdar://problem/20221502>)
- Localization fixes (<rdar://problem/25292403>, <rdar://problem/25461517>,
  Issue #4041, Issue #4796)
- Documentation changes (Issue #4624, Issue #4781)
- Packaging fixes (PR #4832)
