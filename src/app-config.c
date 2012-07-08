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
    FmAppConfig *cfg;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_APP_CONFIG(object));

    cfg = FM_APP_CONFIG(object);
    if(cfg->wallpapers_configured > 0)
    {
        int i;

        for(i = 0; i < cfg->wallpapers_configured; i++)
            g_free(cfg->wallpapers[i]);
        g_free(cfg->wallpapers);
    }
    g_free(cfg->wallpaper);
    g_free(cfg->desktop_font);
    g_free(cfg->su_cmd);

    G_OBJECT_CLASS(fm_app_config_parent_class)->finalize(object);
}


static void fm_app_config_init(FmAppConfig *cfg)
{
    /* load libfm config file */
    fm_config_load_from_file((FmConfig*)cfg, NULL);

    cfg->bm_open_method = FM_OPEN_IN_CURRENT_TAB;

    cfg->mount_on_startup = TRUE;
    cfg->mount_removable = TRUE;
    cfg->autorun = TRUE;

    cfg->desktop_fg.red = cfg->desktop_fg.green = cfg->desktop_fg.blue = 65535;
    cfg->win_width = 640;
    cfg->win_height = 480;
    cfg->splitter_pos = 150;
    cfg->max_tab_chars = 32;

    cfg->side_pane_mode = FM_SP_PLACES;

    cfg->view_mode = FM_FV_ICON_VIEW;
    cfg->show_hidden = FALSE;
    cfg->sort_type = GTK_SORT_ASCENDING;
    cfg->sort_by = COL_FILE_NAME;

    cfg->desktop_sort_type = GTK_SORT_ASCENDING;
    cfg->desktop_sort_by = COL_FILE_MTIME;

    cfg->wallpaper_common = TRUE;
}


FmConfig *fm_app_config_new(void)
{
    return (FmConfig*)g_object_new(FM_APP_CONFIG_TYPE, NULL);
}

void fm_app_config_load_from_key_file(FmAppConfig* cfg, GKeyFile* kf)
{
    char* tmp;
    int tmp_int;
    /* behavior */
    fm_key_file_get_int(kf, "config", "bm_open_method", &cfg->bm_open_method);
    tmp = g_key_file_get_string(kf, "config", "su_cmd", NULL);
    g_free(cfg->su_cmd);
    cfg->su_cmd = tmp;

    /* volume management */
    fm_key_file_get_bool(kf, "volume", "mount_on_startup", &cfg->mount_on_startup);
    fm_key_file_get_bool(kf, "volume", "mount_removable", &cfg->mount_removable);
    fm_key_file_get_bool(kf, "volume", "autorun", &cfg->autorun);

    /* desktop */
    fm_key_file_get_int(kf, "desktop", "wallpaper_mode", &tmp_int);
    cfg->wallpaper_mode = (FmWallpaperMode)tmp_int;

    if(cfg->wallpapers_configured > 0)
    {
        int i;

        for(i = 0; i < cfg->wallpapers_configured; i++)
            g_free(cfg->wallpapers[i]);
        g_free(cfg->wallpapers);
    }
    g_free(cfg->wallpaper);
    fm_key_file_get_int(kf, "desktop", "wallpapers_configured", &cfg->wallpapers_configured);
    if(cfg->wallpapers_configured > 0)
    {
        char wpn_buf[32];
        int i;

        cfg->wallpapers = g_malloc(cfg->wallpapers_configured * sizeof(char *));
        for(i = 0; i < cfg->wallpapers_configured; i++)
        {
            snprintf(wpn_buf, sizeof(wpn_buf), "wallpaper%d", i);
            tmp = g_key_file_get_string(kf, "desktop", wpn_buf, NULL);
            cfg->wallpapers[i] = tmp;
        }
    }
    fm_key_file_get_bool(kf, "desktop", "wallpaper_common", &cfg->wallpaper_common);
    if (cfg->wallpaper_common)
    {
        tmp = g_key_file_get_string(kf, "desktop", "wallpaper", NULL);
        cfg->wallpaper = tmp;
    }

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
    if(fm_key_file_get_int(kf, "desktop", "sort_type", &tmp_int) &&
       tmp_int == GTK_SORT_DESCENDING)
        cfg->desktop_sort_type = GTK_SORT_DESCENDING;
    else
        cfg->desktop_sort_type = GTK_SORT_ASCENDING;
    if(fm_key_file_get_int(kf, "desktop", "sort_by", &tmp_int) &&
       FM_FOLDER_MODEL_COL_IS_VALID((guint)tmp_int))
        cfg->desktop_sort_by = tmp_int;

    /* ui */
    fm_key_file_get_int(kf, "ui", "always_show_tabs", &cfg->always_show_tabs);
    fm_key_file_get_int(kf, "ui", "hide_close_btn", &cfg->hide_close_btn);
    fm_key_file_get_int(kf, "ui", "max_tab_chars", &cfg->max_tab_chars);

    fm_key_file_get_int(kf, "ui", "win_width", &cfg->win_width);
    fm_key_file_get_int(kf, "ui", "win_height", &cfg->win_height);

    fm_key_file_get_int(kf, "ui", "splitter_pos", &cfg->splitter_pos);

    fm_key_file_get_int(kf, "ui", "side_pane_mode", &tmp_int);
    cfg->side_pane_mode = (FmSidePaneMode)tmp_int;

    /* default values for folder views */
    fm_key_file_get_int(kf, "ui", "view_mode", &tmp_int);
    if(!FM_FOLDER_VIEW_MODE_IS_VALID(tmp_int))
        cfg->view_mode = FM_FV_ICON_VIEW;
    else
        cfg->view_mode = tmp_int;
    fm_key_file_get_bool(kf, "ui", "show_hidden", &cfg->show_hidden);
    fm_key_file_get_int(kf, "ui", "sort_type", &tmp_int);
    if(tmp_int == GTK_SORT_DESCENDING)
        cfg->sort_type = GTK_SORT_DESCENDING;
    else
        cfg->sort_type = GTK_SORT_ASCENDING;
    fm_key_file_get_int(kf, "ui", "sort_by", &cfg->sort_by);
    if(!FM_FOLDER_MODEL_COL_IS_VALID(cfg->sort_by))
        cfg->sort_by = COL_FILE_NAME;
}

void fm_app_config_load_from_profile(FmAppConfig* cfg, const char* name)
{
    const gchar * const *dirs, * const *dir;
    char *path;
    GKeyFile* kf = g_key_file_new();
    const char* old_name = name;

    if(!name || !*name) /* if profile name is not provided, use 'default' */
    {
        name = "default";
        old_name = "pcmanfm"; /* for compatibility with old versions. */
    }

    /* load system-wide settings */
    dirs = g_get_system_config_dirs();
    for(dir=dirs;*dir;++dir)
    {
        path = g_build_filename(*dir, "pcmanfm", name, "pcmanfm.conf", NULL);
        if(g_key_file_load_from_file(kf, path, 0, NULL))
            fm_app_config_load_from_key_file(cfg, kf);
        g_free(path);
    }

    /* override system-wide settings with user-specific configuration */

    /* For backward compatibility, try to load old config file and
     * then migrate to new location */
    path = g_strconcat(g_get_user_config_dir(), "/pcmanfm/", old_name, ".conf", NULL);
    if(G_UNLIKELY(g_key_file_load_from_file(kf, path, 0, NULL)))
    {
        char* new_dir;
        /* old config file is found, migrate to new profile format */
        fm_app_config_load_from_key_file(cfg, kf);

        /* create the profile dir */
        new_dir = g_build_filename(g_get_user_config_dir(), "pcmanfm", name, NULL);
        if(g_mkdir_with_parents(new_dir, 0700) == 0)
        {
            /* move the old config file to new location */
            char* new_path = g_build_filename(new_dir, "pcmanfm.conf", NULL);
            rename(path, new_path);
            g_free(new_path);
        }
        g_free(new_dir);
    }
    else
    {
        g_free(path);
        path = g_build_filename(g_get_user_config_dir(), "pcmanfm", name, "pcmanfm.conf", NULL);
        if(g_key_file_load_from_file(kf, path, 0, NULL))
            fm_app_config_load_from_key_file(cfg, kf);
    }
    g_free(path);

    g_key_file_free(kf);

    /* set some additional default values when needed. */
    if(!cfg->desktop_font) /* set a proper desktop font if needed */
        cfg->desktop_font = g_strdup("Sans 12");
}

void fm_app_config_save_profile(FmAppConfig* cfg, const char* name)
{
    char* path = NULL;;
    char* dir_path;

    if(!name || !*name)
        name = "default";

    dir_path = g_build_filename(g_get_user_config_dir(), "pcmanfm", name, NULL);
    if(g_mkdir_with_parents(dir_path, 0700) != -1)
    {
        GString* buf = g_string_sized_new(1024);

        g_string_append(buf, "[config]\n");
        g_string_append_printf(buf, "bm_open_method=%d\n", cfg->bm_open_method);
        if(cfg->su_cmd && *cfg->su_cmd)
            g_string_append_printf(buf, "su_cmd=%s\n", cfg->su_cmd);

        g_string_append(buf, "\n[volume]\n");
        g_string_append_printf(buf, "mount_on_startup=%d\n", cfg->mount_on_startup);
        g_string_append_printf(buf, "mount_removable=%d\n", cfg->mount_removable);
        g_string_append_printf(buf, "autorun=%d\n", cfg->autorun);

        g_string_append(buf, "\n[desktop]\n");
        g_string_append_printf(buf, "wallpaper_mode=%d\n", cfg->wallpaper_mode);
        g_string_append_printf(buf, "wallpaper_common=%d\n", cfg->wallpaper_common);
        if (cfg->wallpapers && cfg->wallpapers_configured > 0)
        {
            int i;

            g_string_append_printf(buf, "wallpapers_configured=%d\n", cfg->wallpapers_configured);
            for (i = 0; i < cfg->wallpapers_configured; i++)
                if (cfg->wallpapers[i])
                    g_string_append_printf(buf, "wallpaper%d=%s\n", i, cfg->wallpapers[i]);
        }
        if (cfg->wallpaper_common)
            g_string_append_printf(buf, "wallpaper=%s\n", cfg->wallpaper ? cfg->wallpaper : "");
        //FIXME: should desktop_bg and wallpaper_mode be set for each desktop too?
        g_string_append_printf(buf, "desktop_bg=#%02x%02x%02x\n", cfg->desktop_bg.red/257, cfg->desktop_bg.green/257, cfg->desktop_bg.blue/257);
        g_string_append_printf(buf, "desktop_fg=#%02x%02x%02x\n", cfg->desktop_fg.red/257, cfg->desktop_fg.green/257, cfg->desktop_fg.blue/257);
        g_string_append_printf(buf, "desktop_shadow=#%02x%02x%02x\n", cfg->desktop_shadow.red/257, cfg->desktop_shadow.green/257, cfg->desktop_shadow.blue/257);
        if(cfg->desktop_font && *cfg->desktop_font)
            g_string_append_printf(buf, "desktop_font=%s\n", cfg->desktop_font);
        g_string_append_printf(buf, "show_wm_menu=%d\n", cfg->show_wm_menu);
        g_string_append_printf(buf, "sort_type=%d\n", cfg->desktop_sort_type);
        g_string_append_printf(buf, "sort_by=%d\n", cfg->desktop_sort_by);

        g_string_append(buf, "\n[ui]\n");
        g_string_append_printf(buf, "always_show_tabs=%d\n", cfg->always_show_tabs);
        g_string_append_printf(buf, "max_tab_chars=%d\n", cfg->max_tab_chars);
        /* g_string_append_printf(buf, "hide_close_btn=%d\n", cfg->hide_close_btn); */
        g_string_append_printf(buf, "win_width=%d\n", cfg->win_width);
        g_string_append_printf(buf, "win_height=%d\n", cfg->win_height);
        g_string_append_printf(buf, "splitter_pos=%d\n", cfg->splitter_pos);
        g_string_append_printf(buf, "side_pane_mode=%d\n", cfg->side_pane_mode);
        g_string_append_printf(buf, "view_mode=%d\n", cfg->view_mode);
        g_string_append_printf(buf, "show_hidden=%d\n", cfg->show_hidden);
        g_string_append_printf(buf, "sort_type=%d\n", cfg->sort_type);
        g_string_append_printf(buf, "sort_by=%d\n", cfg->sort_by);

        path = g_build_filename(dir_path, "pcmanfm.conf", NULL);
        g_file_set_contents(path, buf->str, buf->len, NULL);
        g_string_free(buf, TRUE);
        g_free(path);
    }
    g_free(dir_path);
}

