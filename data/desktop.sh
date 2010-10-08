#!/bin/sh

echo_mime () {
	printf "$i;";
}

MIMETYPES=`grep -v ^# $1`
printf MimeType=;
for i in $MIMETYPES ; do
	echo_mime;
done

# URI scheme handlers
SCHEMES="pnm mms net rtp rtsp mmsh uvox icy icyx"

for i in $SCHEMES ; do
	printf "x-scheme-handler/$i;"
done

echo ""
