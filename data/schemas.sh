#!/bin/sh

OWNER=totem
COMMAND="totem-video-thumbnailer %u %o"

schema()
{
	echo ;
	echo "        <schema>";
	echo "            <key>/schemas/desktop/gnome/thumbnailers/$NAME</key>";
	echo "            <applyto>/desktop/gnome/thumbnailers/$NAME</applyto>";
	echo "            <owner>$OWNER</owner>";
	echo "            <type>$TYPE</type>";
	echo "            <default>$DEFAULT</default>";
	echo "            <locale name=\"C\">";
	echo "                <short></short>";
	echo "                <long></long>";
	echo "            </locale>";
	echo "        </schema>";
	echo;
}

MIMETYPES=`cat $1 | grep -v short_list_application_ids_for_ | grep "\/"`

echo "<gconfschemafile>";
echo "    <schemalist>";

for i in $MIMETYPES ; do
	DIR=`echo $i | sed 's,/,@,'`

	NAME="$DIR/enable";
	TYPE="bool";
	DEFAULT="true";
	schema;

	NAME="$DIR/command";
	TYPE="string";
	DEFAULT="$COMMAND";
	schema;
done

echo "    </schemalist>";
echo "</gconfschemafile>"
    
