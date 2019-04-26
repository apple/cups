#!/bin/sh
#
# Script to simulate Xpdf/Poppler's pdftops program.
#

options=""

while test $# -gt 0; do
	option="$1"
	shift

	case "$option" in
		-expand)
			options="$options fit-to-page"
			;;
		-h)
			echo "Usage: pdftops [options] filename"
			echo "Options:"
			echo "  -expand"
			echo "  -h"
			echo "  -level1"
			echo "  -level2"
			echo "  -level3"
			echo "  -noembtt"
			echo "  -origpagesizes"
			echo "  -paperw width-points"
			echo "  -paperh length-points"
			echo ""
			echo "THIS IS A COMPATIBILITY WRAPPER"
			exit 0
			;;
		-paperw | -paperh)
			# Ignore width/length in points
			shift
			;;
		-*)
			# Ignore everything else
			;;
		*)
			/usr/libexec/cups/filter/cgpdftops job user title 1 "$options" "$option"
			exit $?
			;;
	esac
done
