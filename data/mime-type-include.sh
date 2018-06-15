#!/bin/sh

. `dirname $0`/mime-functions.sh

echo_mime () {
	echo "\"$i\","
}

if [ x"$1" = "x--nautilus" ] ; then
	get_video_mimetypes $2;

	echo "/* generated with mime-type-include.sh in the totem module, don't edit or"
	echo "   commit in the nautilus module without filing a bug against totem */"

	echo "G_GNUC_UNUSED static const char *video_mime_types[] = {"
	for i in $MIMETYPES ; do
		echo_mime;
	done

	echo "NULL"
	echo "};"

	exit 0
fi

MIMETYPES=`grep -v '^#' $1 | grep -v x-content/ | grep -v x-scheme-handler/`

echo "/* generated with mime-types-include.sh, don't edit */"
echo "G_GNUC_UNUSED static const gchar *mime_types[] = {"

for i in $MIMETYPES ; do
	echo_mime;
done

echo "NULL"
echo "};"

get_audio_mimetypes $1;

echo "G_GNUC_UNUSED static const gchar *audio_mime_types[] = {"
for i in $MIMETYPES ; do
	echo_mime;
done

echo "NULL"
echo "};"

get_video_mimetypes $1;

echo "G_GNUC_UNUSED static const gchar *video_mime_types[] = {"
for i in $MIMETYPES ; do
	echo_mime;
done

echo "NULL"
echo "};"
