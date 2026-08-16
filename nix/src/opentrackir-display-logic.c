#include "opentrackir-display-logic.h"

gboolean
opentrackir_preview_should_run (gboolean               camera_enabled,
                                gboolean               video_enabled,
                                gboolean               window_visible,
                                OpentrackirSessionPhase phase)
{
	return camera_enabled &&
	       video_enabled &&
	       window_visible &&
	       (phase == OPENTRACKIR_SESSION_PHASE_STARTING ||
	        phase == OPENTRACKIR_SESSION_PHASE_STREAMING);
}

gboolean
opentrackir_preview_should_copy (gboolean preview_enabled,
                                 guint64  generation,
                                 guint64  last_generation)
{
	return preview_enabled && generation != 0 && generation != last_generation;
}

gboolean
opentrackir_timeout_should_run (gboolean camera_enabled,
                                gboolean timeout_enabled,
                                guint    timeout_seconds)
{
	return camera_enabled && timeout_enabled && timeout_seconds > 0;
}

char *
opentrackir_format_frame_rate (gboolean has_frame_rate,
                               double   frame_rate)
{
	return has_frame_rate ? g_strdup_printf ("%.1f fps", frame_rate) : g_strdup ("—");
}

char *
opentrackir_format_packet_type (gboolean has_packet_type,
                                guint8   packet_type)
{
	return has_packet_type ? g_strdup_printf ("0x%02X", packet_type) : g_strdup ("—");
}

char *
opentrackir_format_centroid (gboolean has_centroid,
                             double   centroid_x,
                             double   centroid_y)
{
	return has_centroid
		? g_strdup_printf ("%.1f, %.1f", centroid_x, centroid_y)
		: g_strdup ("—");
}
