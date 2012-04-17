#!/bin/sh

. `dirname $0`/mime-functions.sh

echo_mime () {
	echo "    \"$i\","
}

MIMETYPES=`grep -v '^#' $1 | grep -v x-content/ | grep -v x-scheme-handler/`

echo "/* generated with mime-type-imports.sh in the totem module, don't edit or"
echo "   commit in the sushi module without filing a bug against totem */"

echo "let videoTypes = ["
for i in $MIMETYPES ; do
	echo_mime;
done
echo "];"

