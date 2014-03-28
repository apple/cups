@rem Script to enable debug logging for IPPTOOL
set CUPS_DEBUG_LOG=ipptool.log
set CUPS_DEBUG_LEVEL=6
set "CUPS_DEBUG_FILTER=^(http|_http|ipp|_ipp|cupsDo|cupsGetResponse|cupsSend|cupsWrite|sspi|_sspi)"

