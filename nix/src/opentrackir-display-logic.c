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

guint
opentrackir_timeout_remaining_seconds (gint64 deadline_microseconds,
                                       gint64 now_microseconds)
{
	gint64 remaining_microseconds;

	remaining_microseconds = deadline_microseconds - now_microseconds;
	if (deadline_microseconds <= 0 || remaining_microseconds <= 0)
		return 0;
	return (guint)((remaining_microseconds + G_USEC_PER_SEC - 1) / G_USEC_PER_SEC);
}

char *
opentrackir_format_timeout_remaining (guint remaining_seconds)
{
	guint hours = remaining_seconds / 3600;
	guint minutes = (remaining_seconds % 3600) / 60;
	guint seconds = remaining_seconds % 60;

	return g_strdup_printf ("Remaining: %02u:%02u:%02u", hours, minutes, seconds);
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
