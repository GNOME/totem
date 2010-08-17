#!/bin/sh

schema()
{
	echo "			<key name='$KEY' type='$TYPE'>";
	echo "				<default>$DEFAULT</default>";
	echo "			</key>";
}

SCHEMES="pnm mms net rtp rtsp mmsh uvox icy icyx"

echo "<schemalist>";

echo "	<schema id='org.gnome.desktop.url-handlers' path='/desktop/gnome/url-handlers/'>";

for i in $SCHEMES ; do
	NAME=`echo $i | sed 's,/,-,' | sed 's,+,-,' | sed 's,\.,-,'`
	echo "		<child name='$NAME' schema='org.gnome.desktop.url-handlers.$NAME'/>";
done

echo "	</schema>";

for i in $SCHEMES ; do
	NAME="$i";

	echo "	<schema id='org.gnome.desktop.url-handlers.$NAME' path='/desktop/gnome/url-handlers/$NAME/'>";

	KEY="command"
	TYPE="s";
	DEFAULT="'totem \"%s\"'";
	schema;

	KEY="needs-terminal"
	TYPE="b";
	DEFAULT="false";
	schema;

	KEY="enabled";
	TYPE="b";
	DEFAULT="true";
	schema;

	echo "	</schema>"
done

echo "</schemalist>"

