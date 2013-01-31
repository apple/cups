#!/bin/sh
#
# Script that builds the print-job tests for all IPP Everywhere PWG Raster
# documents.
#

for file in color.jpg-4x6 document-a4 document-letter gray.jpg-4x6 onepage-a4 onepage-letter; do
    for res in 150dpi 180dpi 300dpi 360dpi 600dpi 720dpi; do
	HAVE_RES="`echo HAVE_$res | awk '{print toupper($1);}'`"
        for type in black-1 cmyk-8 sgray-8 srgb-8 srgb-16; do
            if test -f pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg; then
	            HAVE_TYPE="`echo HAVE_$type | awk '{print toupper($1);}' | tr '-' '_'`"
	            cat <<EOF
{
	NAME "Print $file @ $res, $type"
	SKIP-IF-MISSING pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg
	SKIP-IF-NOT-DEFINED $HAVE_RES
	SKIP-IF-NOT-DEFINED $HAVE_TYPE

	OPERATION Print-Job
	GROUP operation-attributes-tag
	ATTR charset attributes-charset utf-8
	ATTR naturalLanguage attributes-natural-language en
	ATTR uri printer-uri \$uri
	ATTR name requesting-user-name \$user
	ATTR mimeMediaType document-format image/pwg-raster
	ATTR name job-name "$file"
	FILE pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg

	STATUS successful-ok
	STATUS server-error-busy REPEAT-MATCH
}

{
	NAME "Print $file @ $res, $type, deflate"
	SKIP-IF-MISSING pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg
	SKIP-IF-NOT-DEFINED $HAVE_RES
	SKIP-IF-NOT-DEFINED $HAVE_TYPE
	SKIP-IF-NOT-DEFINED HAVE_DEFLATE

	OPERATION Print-Job
	GROUP operation-attributes-tag
	ATTR charset attributes-charset utf-8
	ATTR naturalLanguage attributes-natural-language en
	ATTR uri printer-uri \$uri
	ATTR name requesting-user-name \$user
	ATTR mimeMediaType document-format image/pwg-raster
	ATTR keyword compression deflate
	ATTR name job-name "$file"
	COMPRESSION deflate
	FILE pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg

	STATUS successful-ok
	STATUS server-error-busy REPEAT-MATCH
}

{
	NAME "Print $file @ $res, $type, gzip"
	SKIP-IF-MISSING pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg
	SKIP-IF-NOT-DEFINED $HAVE_RES
	SKIP-IF-NOT-DEFINED $HAVE_TYPE
	SKIP-IF-NOT-DEFINED HAVE_GZIP

	OPERATION Print-Job
	GROUP operation-attributes-tag
	ATTR charset attributes-charset utf-8
	ATTR naturalLanguage attributes-natural-language en
	ATTR uri printer-uri \$uri
	ATTR name requesting-user-name \$user
	ATTR mimeMediaType document-format image/pwg-raster
	ATTR keyword compression gzip
	ATTR name job-name "$file"
	COMPRESSION gzip
	FILE pwg-raster-samples-$res-20111130/$type/$file-$type-$res.pwg

	STATUS successful-ok
	STATUS server-error-busy REPEAT-MATCH
}

EOF
	    fi
	done
    done
done
