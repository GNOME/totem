#!/bin/sh

echo_mime () {
	echo -n "$i;";
}

MIMETYPES=`cat $1`
echo -n MimeType=;
for i in $MIMETYPES ; do
	echo_mime;
done

