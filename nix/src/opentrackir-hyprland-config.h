#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	OPENTRACKIR_HYPRLAND_CONFIG_NONE,
	OPENTRACKIR_HYPRLAND_CONFIG_BINDINGS_LUA,
	OPENTRACKIR_HYPRLAND_CONFIG_MAIN_LUA,
	OPENTRACKIR_HYPRLAND_CONFIG_CLASSIC,
} OpentrackirHyprlandConfigStyle;

gboolean opentrackir_hyprland_desktop_matches (const char *desktop_names);

OpentrackirHyprlandConfigStyle opentrackir_hyprland_config_style (
	gboolean bindings_lua_exists,
	gboolean main_lua_exists,
	gboolean classic_config_exists
);

char *opentrackir_hyprland_find_config (OpentrackirHyprlandConfigStyle *style);

gboolean opentrackir_hyprland_content_has_binding (const char *contents);

char *opentrackir_hyprland_content_with_binding (
	const char                     *contents,
	OpentrackirHyprlandConfigStyle  style,
	gboolean                       *changed
);

gboolean opentrackir_hyprland_install_binding (
	const char                     *path,
	OpentrackirHyprlandConfigStyle  style,
	gboolean                       *changed,
	char                          **backup_path,
	GError                        **error
);

G_END_DECLS
