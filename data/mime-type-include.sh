#!/bin/sh

echo_mime () {
	echo "\"$i\","
}

MIMETYPES=`cat $1`

echo "/* generated with mime-types.sh, don't edit */"
echo "char *mime_types[] = {"

for i in $MIMETYPES ; do
	echo_mime;
done

echo "};"

