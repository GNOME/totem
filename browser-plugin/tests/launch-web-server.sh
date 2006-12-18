#!/bin/sh

# Port to listen on
PORT=12345
# Address to listen on to
# Use the former to allow everyone to access it
# ADDRESS=
ADDRESS=127.0.0.1
# Find Apache first

for i in /usr/sbin/httpd ; do
	if [ -x $i ] ; then
		HTTPD=$i
	fi
done

if [ -z $HTTPD ] ; then
	echo "Could not find httpd at the usual locations"
	exit 1
fi

# Check whether we in the right directory

if [ ! -f `basename $0` ] ; then
	echo "You need to launch `basename $0` from within its directory"
	echo "eg: ./`basename $0` [stop | start]"
	exit 1
fi

ROOTDIR=`dirname $0`/root

# See if we shoud stop the web server

if [ -z $1 ] ; then
	echo "Usage: ./`basename $0` [stop | start]"
	exit 1
fi

if [ $1 = stop ] ; then
	echo "Trying to stop $HTTPD(`cat root/pid`)"
	pushd root/ > /dev/null
	$HTTPD -f `pwd`/conf -k stop
	popd > /dev/null
	exit
fi

# Setup the ServerRoot

if [ ! -d $ROOTDIR ] ; then
	mkdir -p root/ || ( echo "Could not create the ServerRoot" ; exit 1 )
fi

DOCDIR=`pwd`
pushd root/ > /dev/null
# Resolve the relative ROOTDIR path
ROOTDIR=`pwd`
if [ -f pid ] && [ -f conf ] ; then
	$HTTPD -f $ROOTDIR/conf -k stop
	sleep 2
fi
rm -f conf pid lock log access_log

# Setup the config file

echo "LoadModule env_module /etc/httpd/modules/mod_env.so" >> conf
#echo "LoadModule mime_magic_module /etc/httpd/modules/mod_mime_magic.so" >> conf
echo "LoadModule mime_module /etc/httpd/modules/mod_mime.so" >> conf
echo "LoadModule dir_module /etc/httpd/modules/mod_dir.so" >> conf
echo "LoadModule autoindex_module /etc/httpd/modules/mod_autoindex.so" >> conf
echo "LoadModule rewrite_module  /etc/httpd/modules/mod_rewrite.so" >> conf
echo "LoadModule log_config_module /etc/httpd/modules/mod_log_config.so" >> conf

echo "ServerRoot \"$ROOTDIR\""                  >> conf
echo "PidFile pid"                              >> conf
echo "LockFile lock"                            >> conf
#echo "LogLevel crit"                            >> conf
echo "LogLevel info"                            >> conf
echo "ErrorLog log"                             >> conf
echo 'LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" combined' >> conf
echo "CustomLog access_log combined"		>> conf
echo "TypesConfig /etc/mime.types"              >> conf
echo "DocumentRoot \"$DOCDIR\""                 >> conf
echo "<Directory \"$DOCDIR\">"                  >> conf
echo "AllowOverride All"                        >> conf
echo "</Directory>"                             >> conf
echo                                            >> conf
echo "StartServers 1"                           >> conf
echo "MinSpareServers 1"                        >> conf
echo "MaxSpareServers 1"                        >> conf
echo "MaxClients 3"                             >> conf
echo                                            >> conf

popd > /dev/null

# Launch!

#$HTTPD -f $ROOTDIR/conf -C "Listen 127.0.0.1:$PORT"
if [ -z $ADDRESS ] ; then
	$HTTPD -f $ROOTDIR/conf -C "Listen $PORT"
else
	$HTTPD -f $ROOTDIR/conf -C "Listen ${ADDRESS}:$PORT"
fi
echo "Please start debugging at http://localhost:$PORT/"

