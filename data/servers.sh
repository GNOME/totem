#!/bin/sh

echo_mime () {
	echo "<item value=\"$i\"/>"
}

MIMETYPES=`cat $1`
for i in $MIMETYPES ; do
	echo_mime;
done

