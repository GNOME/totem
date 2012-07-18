#!/bin/sh

# Port to listen on
PORT=12345


function usage ()
{
	echo "Usage: ./`basename $0` <--remote> [stop | start]"
	echo " --remote: allow for connections from remote machines"
	exit 1
}


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
	echo "eg: ./`basename $0` <--remote> [stop | start]"
	echo " --remote: allow for connections from remote machines"
	exit 1
fi

ROOTDIR=`dirname $0`/root

# See if we shoud stop the web server

if [ -z $1 ] ; then
	usage $0
fi

if [ x$1 = x"--remote" ] ; then
	ADDRESS=
	shift
else
	ADDRESS=127.0.0.1
fi

if [ x$1 = xstop ] ; then
	echo "Trying to stop $HTTPD(`cat root/pid`)"
	pushd root/ > /dev/null
	$HTTPD -f `pwd`/conf -k stop
	popd > /dev/null
	exit
elif [ x$1 != xstart ] ; then
	usage $0
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

cat > conf << EOF
LoadModule env_module /etc/httpd/modules/mod_env.so
#echo "LoadModule mime_magic_module /etc/httpd/modules/mod_mime_magic.so
LoadModule mime_module /etc/httpd/modules/mod_mime.so
LoadModule dir_module /etc/httpd/modules/mod_dir.so
LoadModule autoindex_module /etc/httpd/modules/mod_autoindex.so
LoadModule rewrite_module  /etc/httpd/modules/mod_rewrite.so
LoadModule log_config_module /etc/httpd/modules/mod_log_config.so

ServerRoot "$ROOTDIR"
PidFile pid
LockFile lock
# LogLevel crit
LogLevel info
ErrorLog log
LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" combined
CustomLog access_log combined
TypesConfig /etc/mime.types
DocumentRoot "$DOCDIR"
<Directory "$DOCDIR">
AllowOverride All
</Directory>

StartServers 1
MinSpareServers 1
MaxSpareServers 1
MaxClients 3
EOF

popd > /dev/null

# Launch!

#$HTTPD -f $ROOTDIR/conf -C "Listen 127.0.0.1:$PORT"
if [ -z $ADDRESS ] ; then
	$HTTPD -f $ROOTDIR/conf -C "Listen $PORT"
else
	$HTTPD -f $ROOTDIR/conf -C "Listen ${ADDRESS}:$PORT"
fi
echo "Please start debugging at http://localhost:$PORT/"

