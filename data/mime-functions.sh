#!/bin/sh

get_audio_mimetypes ()
{
	MIMETYPES=`grep -v ^# $1 | grep "\/" | grep audio | grep -v "audio/x-pn-realaudio"`
	MIMEYPES="$MIMETYPES application/x-flac"
}

get_video_mimetypes ()
{
	MIMETYPES=`grep -v ^# $1 | grep -v x-content/ | grep -v audio | grep -v "application/x-flac"`
	MIMETYPES="$MIMETYPES audio/x-pn-realaudio"
}

