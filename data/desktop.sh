#!/bin/sh

. `dirname $0`/mime-functions.sh

echo_mime () {
	printf "$i;";
}

echo_handler () {
	printf "x-scheme-handler/$i;";
}

get_video_mimetypes $1;
for i in $MIMETYPES ; do
	echo_mime;
done

get_playlist_media_mimetypes $1;
for i in $MIMETYPES ; do
	echo_mime;
done

MIMETYPES=`grep -v ^# $2`
for i in $MIMETYPES ; do
	echo_handler;
done

echo ""
