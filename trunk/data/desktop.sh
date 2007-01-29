#!/bin/sh

echo_mime () {
	printf "$i;";
}

MIMETYPES=`cat $1`
printf MimeType=;
for i in $MIMETYPES ; do
	echo_mime;
done

