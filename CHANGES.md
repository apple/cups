CHANGES - 2.3b1 - 2017-11-01
============================


Changes in CUPS v2.3b1
----------------------

- The lpstat command now reports when new jobs are being held (Issue #4761)
- No longer support backslash, question mark, or quotes in printer names
  (Issue #4966)
- Dropped RSS subscription management from the web interface (Issue #5012)
- The lpadmin command now provides a better error message when an unsupported
  System V interface script is used (Issue #5111)
- Dropped hard-coded CGI scripting language support (Issue #5124)
- Fixed the ippserver sample code when threading is disabled or unavailable
  (Issue #5154)
