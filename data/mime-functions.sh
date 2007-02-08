#!/bin/sh

get_audio_mimetypes ()
{
	MIMETYPES=`cat $1 | grep "\/" | grep audio | grep -v "audio/x-pn-realaudio"`
}

get_video_mimetypes ()
{
	MIMETYPES=`cat $1 | grep -v audio | grep -v "application/x-flac"`
	MIMETYPES="$MIMETYPES audio/x-pn-realaudio"
}

