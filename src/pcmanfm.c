/*
 *      pcmanfm.c
 *      
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
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

#include <config.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include <fm-gtk.h>
#include "app-config.h"
#include "main-win.h"
#include "desktop.h"

static void on_manage_desktop_changed(FmConfig* cfg, gpointer user_data)
{
    if(FM_APP_CONFIG(fm_config)->manage_desktop)
        fm_desktop_manager_init();
    else
        fm_desktop_manager_finalize();
}

int main(int argc, char** argv)
{
	GtkWidget* w;
    FmConfig* config;
	gtk_init(&argc, &argv);

    config = fm_app_config_new();
	fm_gtk_init(config);

    g_signal_connect(fm_config, "changed::manage_desktop", G_CALLBACK(on_manage_desktop_changed), NULL);
    if(FM_APP_CONFIG(fm_config)->manage_desktop)
        fm_desktop_manager_init();

	w = fm_main_win_new();
	gtk_window_set_default_size(w, 640, 480);
	gtk_widget_show(w);

    if(argc > 1)
    {
        FmPath* path = fm_path_new(argv[1]);
        fm_main_win_chdir(w, path);
        fm_path_unref(path);
    }

	gtk_main();

    fm_config_save(config, NULL); /* save libfm config */
    fm_app_config_save((FmAppConfig*)config, NULL); /* save pcmanfm config */

    if(FM_APP_CONFIG(fm_config)->manage_desktop)
        fm_desktop_manager_finalize();

    fm_gtk_finalize();

	return 0;
}
