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

#include <libfm/fm-gtk.h>
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

    self->mount_on_startup = TRUE;
    self->mount_removable = TRUE;
    self->autorun = TRUE;

    self->desktop_fg.red = self->desktop_fg.green = self->desktop_fg.blue = 65535;
    self->win_width = 640;
    self->win_height = 480;
    self->splitter_pos = 150;

    self->view_mode = FM_FV_ICON_VIEW;
    self->show_hidden = FALSE;
    self->sort_type = GTK_SORT_ASCENDING;
    self->sort_by = COL_FILE_NAME;
}


FmConfig *fm_app_config_new(void)
{
    return (FmConfig*)g_object_new(FM_APP_CONFIG_TYPE, NULL);
}

void fm_app_config_load_from_key_file(FmAppConfig* cfg, GKeyFile* kf)
{
    char* tmp;
    /* behavior */
    fm_key_file_get_bool(kf, "config", "bm_open_method", &cfg->bm_open_method);
    cfg->su_cmd = g_key_file_get_string(kf, "config", "su_cmd", NULL);

    /* volume management */
    fm_key_file_get_bool(kf, "volume", "mount_on_startup", &cfg->mount_on_startup);
    fm_key_file_get_bool(kf, "volume", "mount_removable", &cfg->mount_removable);
    fm_key_file_get_bool(kf, "volume", "autorun", &cfg->autorun);

    /* desktop */
    fm_key_file_get_int(kf, "desktop", "wallpaper_mode", &cfg->wallpaper_mode);

    tmp = g_key_file_get_string(kf, "desktop", "wallpaper", NULL);
    g_free(cfg->wallpaper);
    cfg->wallpaper = tmp;

    tmp = g_key_file_get_string(kf, "desktop", "desktop_bg", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_bg);
        g_free(tmp);
    }
    tmp = g_key_file_get_string(kf, "desktop", "desktop_fg", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_fg);
        g_free(tmp);
    }
    tmp = g_key_file_get_string(kf, "desktop", "desktop_shadow", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_shadow);
        g_free(tmp);
    }

    tmp = g_key_file_get_string(kf, "desktop", "desktop_font", NULL);
    g_free(cfg->desktop_font);
    cfg->desktop_font = tmp;

    fm_key_file_get_bool(kf, "desktop", "show_wm_menu", &cfg->show_wm_menu);

    /* ui */
    fm_key_file_get_int(kf, "ui", "always_show_tabs", &cfg->always_show_tabs);
    fm_key_file_get_int(kf, "ui", "hide_close_btn", &cfg->hide_close_btn);

    fm_key_file_get_int(kf, "ui", "win_width", &cfg->win_width);
    fm_key_file_get_int(kf, "ui", "win_height", &cfg->win_height);

    fm_key_file_get_int(kf, "ui", "splitter_pos", &cfg->splitter_pos);

    /* default values for folder views */
    fm_key_file_get_int(kf, "ui", "view_mode", &cfg->view_mode);
    fm_key_file_get_bool(kf, "ui", "show_hidden", &cfg->show_hidden);
    fm_key_file_get_int(kf, "ui", "sort_type", &cfg->sort_type);
    fm_key_file_get_int(kf, "ui", "sort_by", &cfg->sort_by);
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

    dirs = g_get_system_config_dirs();
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
            if(cfg->su_cmd)
                fprintf(f, "su_cmd=%s\n", cfg->su_cmd);

            fputs("\n[volume]\n", f);
            fprintf(f, "mount_on_startup=%d\n", cfg->mount_on_startup);
            fprintf(f, "mount_removable=%d\n", cfg->mount_removable);
            fprintf(f, "autorun=%d\n", cfg->autorun);

            fputs("\n[desktop]\n", f);
            fprintf(f, "wallpaper_mode=%d\n", cfg->wallpaper_mode);
            fprintf(f, "wallpaper=%s\n", cfg->wallpaper ? cfg->wallpaper : "");
            fprintf(f, "desktop_bg=#%02x%02x%02x\n", cfg->desktop_bg.red/257, cfg->desktop_bg.green/257, cfg->desktop_bg.blue/257);
            fprintf(f, "desktop_fg=#%02x%02x%02x\n", cfg->desktop_fg.red/257, cfg->desktop_fg.green/257, cfg->desktop_fg.blue/257);
            fprintf(f, "desktop_shadow=#%02x%02x%02x\n", cfg->desktop_shadow.red/257, cfg->desktop_shadow.green/257, cfg->desktop_shadow.blue/257);
            if(cfg->desktop_font)
                fprintf(f, "desktop_font=%s\n", cfg->desktop_font);
            fprintf(f, "show_wm_menu=%d\n", cfg->show_wm_menu);

            fputs("\n[ui]\n", f);
            fprintf(f, "always_show_tabs=%d\n", cfg->always_show_tabs);
            fprintf(f, "hide_close_btn=%d\n", cfg->hide_close_btn);
            fprintf(f, "win_width=%d\n", cfg->win_width);
            fprintf(f, "win_height=%d\n", cfg->win_height);
            fprintf(f, "splitter_pos=%d\n", cfg->splitter_pos);
            fprintf(f, "view_mode=%d\n", cfg->view_mode);
            fprintf(f, "show_hidden=%d\n", cfg->show_hidden);
            fprintf(f, "sort_type=%d\n", cfg->sort_type);
            fprintf(f, "sort_by=%d\n", cfg->sort_by);
            fclose(f);
        }
    }
    g_free(dir_path);
    g_free(path);
}

