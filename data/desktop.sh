#!/bin/sh

echo_mime () {
	echo -n "$i;";
}

MIMETYPES=`cat $1 | grep -v short_list_application_ids_for_ | grep "\/"`
echo -n MimeType=;
for i in $MIMETYPES ; do
	echo_mime;
done

