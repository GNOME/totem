[CCode (cprefix = "Bacon", lower_case_cprefix = "bacon_")]

namespace Bacon {
	[CCode (cheader_filename = "bacon-video-widget.h")]
	public class VideoWidget : Gtk.EventBox {
    [CCode (has_construct_function = false)]
		public VideoWidget (int width, int height, UseType type) throws GLib.Error;

    public void get_metadata (MetadataType type, out GLib.Value val);
	}
  [CCode (cprefix = "BVW_USE_TYPE_", cheader_filename = "bacon-video-widget.h")]
  public enum UseType {
    VIDEO,
    AUDIO,
    CAPTURE,
    METADATA
  }
  [CCode (cprefix = "BVW_INFO_", cheader_filename = "bacon-video-widget.h")]
  public enum MetadataType {
    TITLE,
    ARTIST,
    YEAR,
    COMMENT,
    ALBUM,
    DURATION,
    TRACK_NUMBER,
    COVER,
    HAS_VIDEO,
    DIMENSION_X,
    DIMENSION_Y,
    VIDEO_BITRATE,
    VIDEO_CODEC,
    FPS,
    HAS_AUDIO,
    AUDIO_BITRATE,
    AUDIO_CODEC,
    AUDIO_SAMPLE_RATE,
    AUDIO_CHANNELS
  }
}
