#!/bin/sh

echo_mime () {
	echo "<item value=\"$i\"/>"
}

MIMETYPES=`cat $1 | grep -v short_list_application_ids_for_ | grep "\/"`
for i in $MIMETYPES ; do
	echo_mime;
done

