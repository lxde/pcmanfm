/*
 *      app-config.c
 *      
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fm-utils.h>
#include <stdio.h>

#include "app-config.h"

static void fm_app_config_finalize              (GObject *object);

G_DEFINE_TYPE(FmAppConfig, fm_app_config, FM_CONFIG_TYPE);


static void fm_app_config_class_init(FmAppConfigClass *klass)
{
    GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_app_config_finalize;
}


static void fm_app_config_finalize(GObject *object)
{
    FmAppConfig *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_APP_CONFIG(object));

    self = FM_APP_CONFIG(object);
    g_free(self->wallpaper);

    G_OBJECT_CLASS(fm_app_config_parent_class)->finalize(object);
}


static void fm_app_config_init(FmAppConfig *self)
{
    /* load libfm config file */
    fm_config_load_from_file((FmConfig*)self, NULL);

    /* load pcmanfm-specific config file */
    fm_app_config_load_from_file(self, "pcmanfm.conf");
}


FmConfig *fm_app_config_new(void)
{
    return (FmConfig*)g_object_new(FM_APP_CONFIG_TYPE, NULL);
}

void fm_app_config_load_from_key_file(FmAppConfig* cfg, GKeyFile* kf)
{
    fm_key_file_get_bool(kf, "config", "bm_open_method", &cfg->bm_open_method);

    fm_key_file_get_bool(kf, "desktop", "manage_desktop", &cfg->manage_desktop);
    fm_key_file_get_bool(kf, "desktop", "wallpaper", &cfg->wallpaper);

    fm_key_file_get_int(kf, "ui", "always_show_tabs", &cfg->always_show_tabs);
    fm_key_file_get_int(kf, "ui", "hide_close_btn", &cfg->hide_close_btn);
}

void fm_app_config_load_from_file(FmAppConfig* cfg, const char* name)
{
    char **dirs, **dir;
    char *path;
    GKeyFile* kf = g_key_file_new();

    if(G_LIKELY(!name))
        name = "pcmanfm/pcmanfm.conf";
    else
    {
        if(G_UNLIKELY(g_path_is_absolute(name)))
        {
            if(g_key_file_load_from_file(kf, name, 0, NULL))
                fm_app_config_load_from_key_file(cfg, kf);
            goto _out;
        }
    }

    dirs = g_get_system_config_dirs(), **dir;
    for(dir=dirs;*dir;++dir)
    {
        path = g_build_filename(*dir, name, NULL);
        if(g_key_file_load_from_file(kf, path, 0, NULL))
            fm_app_config_load_from_key_file(cfg, kf);
        g_free(path);
    }
    path = g_build_filename(g_get_user_config_dir(), name, NULL);
    if(g_key_file_load_from_file(kf, path, 0, NULL))
        fm_app_config_load_from_key_file(cfg, kf);
    g_free(path);

_out:
    g_key_file_free(kf);
}

void fm_app_config_save(FmAppConfig* cfg, const char* name)
{
    char* path = NULL;;
    char* dir_path;
    FILE* f;
    if(!name)
        name = path = g_build_filename(g_get_user_config_dir(), "pcmanfm/pcmanfm.conf", NULL);
    else if(!g_path_is_absolute(name))
        name = path = g_build_filename(g_get_user_config_dir(), name, NULL);

    dir_path = g_path_get_dirname(name);
    if(g_mkdir_with_parents(dir_path, 0700) != -1)
    {
        f = fopen(name, "w");
        if(f)
        {
            fputs("[config]\n", f);
            fprintf(f, "bm_open_method=%d\n", cfg->bm_open_method);
            fputs("[desktop]\n", f);
            fprintf(f, "manage_desktop=%d\n", cfg->manage_desktop);
            fprintf(f, "wallpaper=%s\n", cfg->wallpaper ? cfg->wallpaper : "");
            fputs("\n[ui]\n", f);
            fprintf(f, "always_show_tabs=%d\n", cfg->always_show_tabs);
            fprintf(f, "hide_close_btn=%d\n", cfg->hide_close_btn);
            fclose(f);
        }
    }
    g_free(dir_path);
    g_free(path);
}

