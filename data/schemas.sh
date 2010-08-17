#!/bin/sh

COMMAND="$2/totem-video-thumbnailer -s %s %u %o"

. `dirname $0`/mime-functions.sh

upd_schema()
{
	echo "gsettings set org.gnome.desktop.thumbnailers.$DIR $NAME \"$DEFAULT\"" 1>&3
}

schema()
{
	echo "		<key name='$NAME' type='$TYPE'>";
	echo "			<default>$DEFAULT</default>";
	echo "		</key>";

	upd_schema;
}


get_video_mimetypes $1;

echo "<schemalist>";

echo "	<schema id='org.gnome.desktop.thumbnailers' path='/desktop/gnome/thumbnailers/'>";

for i in $MIMETYPES ; do
	NAME=`echo $i | sed 's,/,-,' | sed 's,+,-,' | sed 's,\.,-,'`
	echo "		<child name='$NAME' schema='org.gnome.desktop.thumbnailers.$NAME'/>";
done

echo "	</schema>";

for i in $MIMETYPES ; do
	DIR=`echo $i | sed 's,/,-,' | sed 's,+,-,' | sed 's,\.,-,'`

	echo "	<schema id='org.gnome.desktop.thumbnailers.$DIR' path='/desktop/gnome/thumbnailers/$DIR/'>";

	NAME="enable";
	TYPE="b";
	DEFAULT="true";
	schema;

	NAME="command";
	TYPE="s";
	DEFAULT="'$COMMAND'";
	schema;

	echo "	</schema>";
done

get_audio_mimetypes $1;

for i in $MIMETYPES ; do
	DIR=`echo $i | sed 's,/,-,' | sed 's,+,-,' | sed 's,\.,-,'`

	echo "	<schema id='org.gnome.desktop.thumbnailers.$DIR' path='/desktop/gnome/thumbnailers/$DIR/'>";

	NAME="enable";
	TYPE="b";
	DEFAULT="false";
	schema;

	NAME="command";
	TYPE="s";
	DEFAULT="'$COMMAND'";
	schema;

	echo "	</schema>";
done

echo "</schemalist>"

