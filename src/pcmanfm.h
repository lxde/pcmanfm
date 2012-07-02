/*
 *      pcmanfm.h
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef __PCMANFM_H__
#define __PCMANFM_H__

#include <gtk/gtk.h>
#include <libfm/fm.h>

G_BEGIN_DECLS

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref();

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
void pcmanfm_unref();

gboolean pcmanfm_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err);

char* pcmanfm_get_profile_dir(gboolean create);
void pcmanfm_save_config(gboolean immediate);

void pcmanfm_open_folder_in_terminal(GtkWindow* parent, FmPath* dir);

#define TEMPL_NAME_FOLDER    NULL
#define TEMPL_NAME_BLANK     (const char*)-1
#define TEMPL_NAME_SHORTCUT  (const char*)-2
void pcmanfm_create_new(GtkWindow* parent, FmPath* cwd, const char* templ);

G_END_DECLS

#endif
