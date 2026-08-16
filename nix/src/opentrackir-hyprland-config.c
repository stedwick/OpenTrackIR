#include "opentrackir-hyprland-config.h"

#include <gio/gio.h>
#include <string.h>

#define HOTKEY_ACTION "org.gnome.opentrackir:toggle-mouse"

static const char lua_binding[] =
	"-- OpenTrackIR global hotkey.\n"
	"o.bind(\n"
	"  \"SHIFT + F7\",\n"
	"  \"Toggle OpenTrackIR mouse movement\",\n"
	"  hl.dsp.global(\"" HOTKEY_ACTION "\")\n"
	")\n";

static const char classic_binding[] =
	"# OpenTrackIR global hotkey.\n"
	"bind = SHIFT, F7, global, " HOTKEY_ACTION "\n";

static gboolean reload_and_validate (GError **error);
static gboolean restore_backup (const char *path, const char *backup_path, GError **error);

gboolean
opentrackir_hyprland_desktop_matches (const char *desktop_names)
{
	g_auto(GStrv) names = NULL;

	if (desktop_names == NULL || desktop_names[0] == '\0')
		return FALSE;

	names = g_strsplit (desktop_names, ":", -1);
	for (guint index = 0; names[index] != NULL; index++)
	{
		if (g_ascii_strcasecmp (names[index], "Hyprland") == 0)
			return TRUE;
	}

	return FALSE;
}

OpentrackirHyprlandConfigStyle
opentrackir_hyprland_config_style (gboolean bindings_lua_exists,
                                   gboolean main_lua_exists,
                                   gboolean classic_config_exists)
{
	if (bindings_lua_exists)
		return OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA;
	if (main_lua_exists)
		return OPENTRACKIR_HYPRLAND_CONFIG_MAIN_LUA;
	if (classic_config_exists)
		return OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC;
	return OPENTRACKIR_HYPRLAND_CONFIG_NONE;
}

char *
opentrackir_hyprland_find_config (OpentrackirHyprlandConfigStyle *style)
{
	g_autofree char *config_dir = NULL;
	g_autofree char *bindings_lua = NULL;
	g_autofree char *main_lua = NULL;
	g_autofree char *classic_config = NULL;
	OpentrackirHyprlandConfigStyle selected_style;

	g_return_val_if_fail (style != NULL, NULL);

	config_dir = g_build_filename (g_get_user_config_dir (), "hypr", NULL);
	bindings_lua = g_build_filename (config_dir, "bindings.lua", NULL);
	main_lua = g_build_filename (config_dir, "hyprland.lua", NULL);
	classic_config = g_build_filename (config_dir, "hyprland.conf", NULL);
	selected_style = opentrackir_hyprland_config_style (
		g_file_test (bindings_lua, G_FILE_TEST_IS_REGULAR),
		g_file_test (main_lua, G_FILE_TEST_IS_REGULAR),
		g_file_test (classic_config, G_FILE_TEST_IS_REGULAR)
	);
	*style = selected_style;

	switch (selected_style)
	{
	case OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA:
		return g_steal_pointer (&bindings_lua);
	case OPENTRACKIR_HYPRLAND_CONFIG_MAIN_LUA:
		return g_steal_pointer (&main_lua);
	case OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC:
		return g_steal_pointer (&classic_config);
	case OPENTRACKIR_HYPRLAND_CONFIG_NONE:
	default:
		return NULL;
	}
}

gboolean
opentrackir_hyprland_content_has_binding (const char *contents)
{
	return contents != NULL && strstr (contents, HOTKEY_ACTION) != NULL;
}

char *
opentrackir_hyprland_content_with_binding (const char                     *contents,
                                           OpentrackirHyprlandConfigStyle  style,
                                           gboolean                       *changed)
{
	const char *binding;
	g_autoptr(GString) output = NULL;

	g_return_val_if_fail (contents != NULL, NULL);
	g_return_val_if_fail (changed != NULL, NULL);

	if (opentrackir_hyprland_content_has_binding (contents))
	{
		*changed = FALSE;
		return g_strdup (contents);
	}

	switch (style)
	{
	case OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA:
	case OPENTRACKIR_HYPRLAND_CONFIG_MAIN_LUA:
		binding = lua_binding;
		break;
	case OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC:
		binding = classic_binding;
		break;
	case OPENTRACKIR_HYPRLAND_CONFIG_NONE:
	default:
		*changed = FALSE;
		return NULL;
	}

	output = g_string_new (contents);
	if (output->len > 0 && output->str[output->len - 1] != '\n')
		g_string_append_c (output, '\n');
	if (output->len > 0)
		g_string_append_c (output, '\n');
	g_string_append (output, binding);
	*changed = TRUE;
	return g_string_free (g_steal_pointer (&output), FALSE);
}

gboolean
opentrackir_hyprland_install_binding (const char                     *path,
                                      OpentrackirHyprlandConfigStyle  style,
                                      gboolean                       *changed,
                                      char                          **backup_path,
                                      GError                        **error)
{
	g_autofree char *contents = NULL;
	g_autofree char *updated = NULL;
	g_autofree char *local_backup_path = NULL;
	g_autoptr(GError) validation_error = NULL;
	gsize length;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (changed != NULL, FALSE);
	g_return_val_if_fail (backup_path != NULL, FALSE);

	*changed = FALSE;
	*backup_path = NULL;
	if (!g_file_get_contents (path, &contents, &length, error))
		return FALSE;

	updated = opentrackir_hyprland_content_with_binding (contents, style, changed);
	if (updated == NULL)
	{
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_NOT_SUPPORTED,
		                     "The Hyprland configuration format is not supported.");
		return FALSE;
	}
	if (!*changed)
		return TRUE;

	local_backup_path = g_strdup_printf ("%s.bak.%" G_GINT64_FORMAT,
	                                     path,
	                                     g_get_real_time ());
	if (!g_file_set_contents_full (local_backup_path,
	                               contents,
	                               (gssize)length,
	                               G_FILE_SET_CONTENTS_CONSISTENT |
	                               G_FILE_SET_CONTENTS_DURABLE,
	                               0666,
	                               error))
		return FALSE;

	if (!g_file_set_contents_full (path,
	                               updated,
	                               -1,
	                               G_FILE_SET_CONTENTS_CONSISTENT |
	                               G_FILE_SET_CONTENTS_DURABLE,
	                               0666,
	                               error))
		return FALSE;

	if (!reload_and_validate (&validation_error))
	{
		g_autoptr(GError) restore_error = NULL;

		if (!restore_backup (path, local_backup_path, &restore_error))
		{
			g_set_error (error,
			             G_IO_ERROR,
			             G_IO_ERROR_FAILED,
			             "%s The original configuration could not be restored: %s",
			             validation_error->message,
			             restore_error->message);
		}
		else
		{
			(void)reload_and_validate (NULL);
			g_propagate_error (error, g_steal_pointer (&validation_error));
		}
		return FALSE;
	}

	*backup_path = g_steal_pointer (&local_backup_path);
	return TRUE;
}

static gboolean
reload_and_validate (GError **error)
{
	g_autofree char *standard_output = NULL;
	g_autofree char *standard_error = NULL;
	int wait_status;
	const char *reload_argv[] = { "hyprctl", "reload", NULL };
	const char *errors_argv[] = { "hyprctl", "configerrors", NULL };

	if (!g_spawn_sync (NULL,
	                   (char **)reload_argv,
	                   NULL,
	                   G_SPAWN_SEARCH_PATH,
	                   NULL,
	                   NULL,
	                   &standard_output,
	                   &standard_error,
	                   &wait_status,
	                   error) ||
	    !g_spawn_check_wait_status (wait_status, error))
		return FALSE;

	g_clear_pointer (&standard_output, g_free);
	g_clear_pointer (&standard_error, g_free);
	if (!g_spawn_sync (NULL,
	                   (char **)errors_argv,
	                   NULL,
	                   G_SPAWN_SEARCH_PATH,
	                   NULL,
	                   NULL,
	                   &standard_output,
	                   &standard_error,
	                   &wait_status,
	                   error) ||
	    !g_spawn_check_wait_status (wait_status, error))
		return FALSE;

	g_strstrip (standard_output);
	if (standard_output[0] != '\0')
	{
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_FAILED,
		             "Hyprland rejected the configuration: %s",
		             standard_output);
		return FALSE;
	}

	return TRUE;
}

static gboolean
restore_backup (const char *path,
                const char *backup_path,
                GError    **error)
{
	g_autofree char *contents = NULL;
	gsize length;

	if (!g_file_get_contents (backup_path, &contents, &length, error))
		return FALSE;
	return g_file_set_contents_full (path,
	                                 contents,
	                                 (gssize)length,
	                                 G_FILE_SET_CONTENTS_CONSISTENT |
	                                 G_FILE_SET_CONTENTS_DURABLE,
	                                 0666,
	                                 error);
}
