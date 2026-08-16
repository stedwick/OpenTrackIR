#include "opentrackir-status-notifier-icon.h"

#include <string.h>

GVariant *
opentrackir_status_notifier_icon_pixmaps_new (const guint8 *argb32,
                                               gint          width,
                                               gint          height,
                                               gsize         stride)
{
	GVariantBuilder pixmaps;
	g_autofree guint8 *packed = NULL;
	gsize packed_size;
	gsize row_size;
	gint row;

	g_return_val_if_fail (argb32 != NULL, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	row_size = (gsize)width * 4;
	g_return_val_if_fail (stride >= row_size, NULL);
	packed_size = row_size * (gsize)height;
	packed = g_malloc (packed_size);
	for (row = 0; row < height; row++)
	{
		memcpy (packed + ((gsize)row * row_size),
		        argb32 + ((gsize)row * stride),
		        row_size);
	}

	g_variant_builder_init (&pixmaps, G_VARIANT_TYPE ("a(iiay)"));
	g_variant_builder_add (
		&pixmaps,
		"(ii@ay)",
		width,
		height,
		g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
		                           packed,
		                           packed_size,
		                           sizeof (guint8))
	);
	return g_variant_builder_end (&pixmaps);
}
