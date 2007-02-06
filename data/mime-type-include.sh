#!/bin/sh

. mime-functions.sh

echo_mime () {
	echo "\"$i\","
}

MIMETYPES=`cat $1`

echo "/* generated with mime-types-include.sh, don't edit */"
echo "char *mime_types[] = {"

for i in $MIMETYPES ; do
	echo_mime;
done

echo "};"

get_audio_mimetypes $1;

echo "char *audio_mime_types[] = {"
for i in $MIMETYPES ; do
	echo_mime;
done

echo "};"

get_video_mimetypes $1;

echo "char *video_mime_types[] = {"
for i in $MIMETYPES ; do
	echo_mime;
done

echo "};"

