/* opentrackir-application.h
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_APPLICATION (opentrackir_application_get_type())

G_DECLARE_FINAL_TYPE (OpentrackirApplication, opentrackir_application, OPENTRACKIR, APPLICATION, AdwApplication)

OpentrackirApplication *opentrackir_application_new (const char        *application_id,
                                                     GApplicationFlags  flags);
gboolean opentrackir_application_status_notifier_is_available
                                                       (OpentrackirApplication *self);
guint    opentrackir_application_timeout_remaining_seconds
                                                       (OpentrackirApplication *self);

G_END_DECLS
