#!/bin/sh

get_audio_mimetypes ()
{
	MIMETYPES=`grep -v '^#' $1 | grep "\/" | grep audio | grep -v "audio/x-pn-realaudio" | grep -v "audio/x-scpls" | grep -v "audio/mpegurl" | grep -v "audio/x-mpegurl" | grep -v x-scheme-handler/`
	MIMETYPES="$MIMETYPES application/x-flac"
}

get_video_mimetypes ()
{
	MIMETYPES=`grep -v '^#' $1 | grep -v x-content/ | grep -v audio | grep -v "application/x-flac" | grep -v "text/google-video-pointer" | grep -v "application/x-quicktime-media-link" | grep -v -E 'application/.*smil.*' | grep -v "application/xspf+xml" | grep -v "mpegurl" | grep -v -E 'application/[a-z-]*ogg'`
	MIMETYPES="$MIMETYPES audio/x-pn-realaudio"
}

get_playlist_media_mimetypes ()
{
	MIMETYPES=`grep -v '^#' $1 | grep -E "text/google-video-pointer|application/x-quicktime-media-link|application/smil|application/smil+xml|application/x-smil|application/xspf+xml|x-content/*"`
}
