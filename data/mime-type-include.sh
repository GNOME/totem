#!/bin/sh

. `dirname $0`/mime-functions.sh

echo_mime () {
	echo "\"$i\","
}

if [ x"$1" == "x--nautilus" ] ; then
	get_audio_mimetypes $2;

	echo "/* generated with mime-types-include.sh in the totem module, don't edit or "
	echo "   commit in the nautilus module without filing a bug against totem */"

	echo "static char *audio_mime_types[] = {"
	for i in $MIMETYPES ; do
		echo_mime;
	done

	echo "};"

	exit 0
fi

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

