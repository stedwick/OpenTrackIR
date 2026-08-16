#include <gio/gio.h>

#include "opentrackir-status-notifier-icon.h"

static void
test_pixmap_packs_rows_without_stride_padding (void)
{
	const guint8 source[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 99, 99, 99, 99,
		9, 10, 11, 12, 13, 14, 15, 16, 99, 99, 99, 99,
	};
	const guint8 expected[] = {
		1, 2, 3, 4, 5, 6, 7, 8,
		9, 10, 11, 12, 13, 14, 15, 16,
	};
	g_autoptr(GVariant) pixmaps = NULL;
	g_autoptr(GVariant) pixels = NULL;
	const guint8 *actual;
	gsize actual_size;
	gint height;
	gint width;

	pixmaps = g_variant_ref_sink (
		opentrackir_status_notifier_icon_pixmaps_new (source, 2, 2, 12));
	g_assert_cmpuint (g_variant_n_children (pixmaps), ==, 1);
	g_variant_get_child (pixmaps, 0, "(ii@ay)", &width, &height, &pixels);
	g_assert_cmpint (width, ==, 2);
	g_assert_cmpint (height, ==, 2);
	actual = g_variant_get_fixed_array (pixels, &actual_size, sizeof (guint8));
	g_assert_cmpuint (actual_size, ==, sizeof (expected));
	g_assert_cmpmem (actual, actual_size, expected, sizeof (expected));
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/status-notifier/icon-pixmap",
	                 test_pixmap_packs_rows_without_stride_padding);

	return g_test_run ();
}
