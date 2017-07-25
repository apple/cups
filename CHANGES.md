CHANGES - 2.2.5 - 2017-07-23
============================

CHANGES IN CUPS V2.2.5
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


CHANGES IN CUPS V2.2.4
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


CHANGES IN CUPS V2.2.3
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


CHANGES IN CUPS V2.2.2
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


CHANGES IN CUPS V2.2.1
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


CHANGES IN CUPS V2.2.0
----------------------

- Normalized the TLS certificate validation code and added additional error
  messages to aid troubleshooting.
- The httpConnect functions did not work on Linux when cupsd was not running
  (Issue #4870)
- The --no-remote-any option of cupsctl had no effect (Issue #4866)
- http*Connect did not return early when all addresses failed (Issue #4870)


CHANGES IN CUPS V2.2rc1
-----------------------

- Updated the list of supported IPP Everywhere media types.
- The IPP backend did not validate TLS credentials properly.
- The printer-state-message attribute was not cleared after a print job with no
  errors (Issue #4851)
- The CUPS-Add-Modify-Class and CUPS-Add-Modify-Printer operations did not
  always return an error for failed adds (Issue #4854)
- PPD files with names longer than 127 bytes did not work (Issue #4860)
- Updated localizations (Issue #4846, PR #4858)


CHANGES IN CUPS V2.2b2
----------------------

- Added Upstart support (PR #4825)
- CUPS now supports Let's Encrypt certificates on Linux.


CHANGES IN CUPS V2.2b1
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
