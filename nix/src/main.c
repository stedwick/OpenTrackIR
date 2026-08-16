/* main.c
 *
 * Copyright 2026 Philip Brocoum
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <signal.h>

#include <glib/gi18n.h>
#include <glib-unix.h>

#include "opentrackir-application.h"

static gboolean
quit_on_unix_signal (gpointer user_data)
{
	g_application_quit (G_APPLICATION (user_data));
	return G_SOURCE_CONTINUE;
}

int
main (int   argc,
      char *argv[])
{
	g_autoptr(OpentrackirApplication) app = NULL;
	guint sigint_source_id;
	guint sigterm_source_id;
	int ret;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	app = opentrackir_application_new ("org.gnome.opentrackir", G_APPLICATION_DEFAULT_FLAGS);
	sigint_source_id = g_unix_signal_add (SIGINT, quit_on_unix_signal, app);
	sigterm_source_id = g_unix_signal_add (SIGTERM, quit_on_unix_signal, app);
	ret = g_application_run (G_APPLICATION (app), argc, argv);
	if (sigint_source_id != 0)
		g_source_remove (sigint_source_id);
	if (sigterm_source_id != 0)
		g_source_remove (sigterm_source_id);

	return ret;
}
