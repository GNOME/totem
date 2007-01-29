#!/bin/sh

OWNER=totem
COMMAND="$2/totem-video-thumbnailer -s %s %u %o"

upd_schema()
{
	echo "gconftool-2 --set --type $TYPE /desktop/gnome/thumbnailers/$NAME \"$DEFAULT\"" 1>&2
}

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

	upd_schema;
}

MIMETYPES=`cat $1 | grep -v audio | grep -v "application/x-flac" | sed 's,+,@,'`
MIMETYPES="$MIMETYPES audio/x-pn-realaudio"

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

MIMETYPES=`cat $1 | grep -v | grep "\/" | grep audio | grep -v "audio/x-pn-realaudio"`

for i in $MIMETYPES ; do
	DIR=`echo $i | sed 's,/,@,'`
	NAME="$DIR/enable";
	TYPE="bool";
	DEFAULT="false";
	upd_schema;

	NAME="$DIR/command";
	TYPE="string";
	DEFAULT="$COMMAND";
	upd_schema;
done

echo "    </schemalist>";
echo "</gconfschemafile>"
    
