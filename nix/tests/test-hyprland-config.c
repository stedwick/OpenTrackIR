#include <glib.h>
#include <glib/gstdio.h>

#include <string.h>

#include "opentrackir-hyprland-config.h"

static void
test_desktop_detection (void)
{
	g_assert_true (opentrackir_hyprland_desktop_matches ("Hyprland"));
	g_assert_true (opentrackir_hyprland_desktop_matches ("omarchy:Hyprland"));
	g_assert_true (opentrackir_hyprland_desktop_matches ("HYPRLAND"));
	g_assert_false (opentrackir_hyprland_desktop_matches (NULL));
	g_assert_false (opentrackir_hyprland_desktop_matches (""));
	g_assert_false (opentrackir_hyprland_desktop_matches ("GNOME"));
	g_assert_false (opentrackir_hyprland_desktop_matches ("NotHyprland"));
}

static void
test_config_style (void)
{
	g_assert_cmpint (
		opentrackir_hyprland_config_style (TRUE, TRUE, TRUE),
		==,
		OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA
	);
	g_assert_cmpint (
		opentrackir_hyprland_config_style (FALSE, TRUE, TRUE),
		==,
		OPENTRACKIR_HYPRLAND_CONFIG_MAIN_LUA
	);
	g_assert_cmpint (
		opentrackir_hyprland_config_style (FALSE, FALSE, TRUE),
		==,
		OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC
	);
	g_assert_cmpint (
		opentrackir_hyprland_config_style (FALSE, FALSE, FALSE),
		==,
		OPENTRACKIR_HYPRLAND_CONFIG_NONE
	);
}

static void
test_lua_binding (void)
{
	g_autofree char *updated = NULL;
	gboolean changed;

	updated = opentrackir_hyprland_content_with_binding (
		"-- Existing settings.\n",
		OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA,
		&changed
	);
	g_assert_true (changed);
	g_assert_nonnull (strstr (updated, "o.bind("));
	g_assert_nonnull (strstr (updated, "\"SHIFT + F7\""));
	g_assert_nonnull (strstr (
		updated,
		"hl.dsp.global(\"org.gnome.opentrackir:toggle-mouse\")"
	));
}

static void
test_classic_binding (void)
{
	g_autofree char *updated = NULL;
	gboolean changed;

	updated = opentrackir_hyprland_content_with_binding (
		"monitor = , preferred, auto, 1",
		OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC,
		&changed
	);
	g_assert_true (changed);
	g_assert_nonnull (strstr (
		updated,
		"bind = SHIFT, F7, global, org.gnome.opentrackir:toggle-mouse"
	));
	g_assert_true (g_str_has_suffix (updated, "\n"));
}

static void
test_existing_binding_is_unchanged (void)
{
	const char *contents =
		"o.bind(\"SHIFT + F7\", nil, "
		"hl.dsp.global(\"org.gnome.opentrackir:toggle-mouse\"))\n";
	g_autofree char *updated = NULL;
	gboolean changed;

	g_assert_true (opentrackir_hyprland_content_has_binding (contents));
	updated = opentrackir_hyprland_content_with_binding (
		contents,
		OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA,
		&changed
	);
	g_assert_false (changed);
	g_assert_cmpstr (updated, ==, contents);
}

static void
test_install_binding_creates_backup (void)
{
	const char *original = "-- Existing settings.\n";
	g_autofree char *temporary_dir = NULL;
	g_autofree char *config_path = NULL;
	g_autofree char *hyprctl_path = NULL;
	g_autofree char *backup_path = NULL;
	g_autofree char *first_backup_path = NULL;
	g_autofree char *installed = NULL;
	g_autofree char *backup = NULL;
	g_autofree char *original_path = g_strdup (g_getenv ("PATH"));
	g_autoptr(GError) error = NULL;
	gboolean changed;

	temporary_dir = g_dir_make_tmp ("opentrackir-hyprland-test-XXXXXX", &error);
	g_assert_no_error (error);
	g_assert_nonnull (temporary_dir);
	config_path = g_build_filename (temporary_dir, "bindings.lua", NULL);
	hyprctl_path = g_build_filename (temporary_dir, "hyprctl", NULL);
	g_assert_true (g_file_set_contents (config_path, original, -1, &error));
	g_assert_no_error (error);
	g_assert_true (g_file_set_contents (hyprctl_path, "#!/bin/sh\nexit 0\n", -1, &error));
	g_assert_no_error (error);
	g_assert_cmpint (g_chmod (hyprctl_path, 0700), ==, 0);
	g_setenv ("PATH", temporary_dir, TRUE);

	g_assert_true (opentrackir_hyprland_install_binding (
		config_path,
		OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA,
		&changed,
		&backup_path,
		&error
	));
	g_assert_no_error (error);
	g_assert_true (changed);
	g_assert_nonnull (backup_path);
	g_assert_true (g_file_get_contents (config_path, &installed, NULL, &error));
	g_assert_no_error (error);
	g_assert_true (opentrackir_hyprland_content_has_binding (installed));
	g_assert_true (g_file_get_contents (backup_path, &backup, NULL, &error));
	g_assert_no_error (error);
	g_assert_cmpstr (backup, ==, original);

	first_backup_path = g_steal_pointer (&backup_path);
	g_assert_true (opentrackir_hyprland_install_binding (
		config_path,
		OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA,
		&changed,
		&backup_path,
		&error
	));
	g_assert_no_error (error);
	g_assert_false (changed);
	g_assert_null (backup_path);

	if (original_path != NULL)
		g_setenv ("PATH", original_path, TRUE);
	else
		g_unsetenv ("PATH");
	g_assert_cmpint (g_remove (config_path), ==, 0);
	g_assert_cmpint (g_remove (first_backup_path), ==, 0);
	g_assert_cmpint (g_remove (hyprctl_path), ==, 0);
	g_assert_cmpint (g_rmdir (temporary_dir), ==, 0);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/hyprland-config/desktop-detection", test_desktop_detection);
	g_test_add_func ("/hyprland-config/style", test_config_style);
	g_test_add_func ("/hyprland-config/lua-binding", test_lua_binding);
	g_test_add_func ("/hyprland-config/classic-binding", test_classic_binding);
	g_test_add_func ("/hyprland-config/existing-binding", test_existing_binding_is_unchanged);
	g_test_add_func ("/hyprland-config/install-backup", test_install_binding_creates_backup);

	return g_test_run ();
}
