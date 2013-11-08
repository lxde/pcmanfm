/*
 *      app-config.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
 *      Copyright 2012-2013 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#if FM_CHECK_VERSION(1, 0, 2)
static void _parse_sort(GKeyFile *kf, const char *group, FmSortMode *mode, FmFolderModelCol *col)
{
    int tmp_int;

    /* FIXME: parse "sort" strings list first */
    /* parse fallback old style sort config */
    if(fm_key_file_get_int(kf, group, "sort_type", &tmp_int) &&
       tmp_int == GTK_SORT_DESCENDING)
        *mode = FM_SORT_DESCENDING;
    else
        *mode = FM_SORT_ASCENDING;
    if(fm_key_file_get_int(kf, group, "sort_by", &tmp_int) &&
#if FM_CHECK_VERSION(1, 2, 0)
       fm_folder_model_col_is_valid((guint)tmp_int))
#else
       FM_FOLDER_MODEL_COL_IS_VALID((guint)tmp_int))
#endif
        *col = tmp_int;
}
#else /* < 1.0.2 */
static void _parse_sort(GKeyFile *kf, const char *group, GtkSortType *mode, int *col)
{
    int tmp_int;

    if(fm_key_file_get_int(kf, group, "sort_type", &tmp_int) &&
       tmp_int == GTK_SORT_DESCENDING)
        *mode = GTK_SORT_DESCENDING;
    else
        *mode = GTK_SORT_ASCENDING;
    if(fm_key_file_get_int(kf, group, "sort_by", &tmp_int) &&
       FM_FOLDER_MODEL_COL_IS_VALID((guint)tmp_int))
        *col = tmp_int;
}
#endif

#if FM_CHECK_VERSION(1, 0, 2)
static void _save_sort(GString *buf, FmSortMode mode, FmFolderModelCol col)
{
    /* FIXME: save "sort" strings list instead */
    g_string_append_printf(buf, "sort_type=%d\n", FM_SORT_IS_ASCENDING(mode) ? 0 : 1);
    g_string_append_printf(buf, "sort_by=%d\n", col);
}
#else
static void _save_sort(GString *buf, GtkSortType type, int col)
{
    g_string_append_printf(buf, "sort_type=%d\n", sort_type);
    g_string_append_printf(buf, "sort_by=%d\n", col);
}
#endif


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
    if (cfg->desktop_section.configured)
    {
      if(cfg->desktop_section.wallpapers_configured > 0)
      {
        int i;

        for(i = 0; i < cfg->desktop_section.wallpapers_configured; i++)
            g_free(cfg->desktop_section.wallpapers[i]);
        g_free(cfg->desktop_section.wallpapers);
      }
      g_free(cfg->desktop_section.wallpaper);
      g_free(cfg->desktop_section.desktop_font);
    }
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

    cfg->desktop_section.desktop_fg.red = cfg->desktop_section.desktop_fg.green = cfg->desktop_section.desktop_fg.blue = 65535;
    cfg->win_width = 640;
    cfg->win_height = 480;
    cfg->splitter_pos = 150;
    cfg->max_tab_chars = 32;

    cfg->side_pane_mode = FM_SP_PLACES;

    cfg->view_mode = FM_FV_ICON_VIEW;
    cfg->show_hidden = FALSE;
#if FM_CHECK_VERSION(1, 0, 2)
    cfg->sort_type = FM_SORT_ASCENDING;
    cfg->sort_by = FM_FOLDER_MODEL_COL_NAME;
    cfg->desktop_section.desktop_sort_type = FM_SORT_ASCENDING;
    cfg->desktop_section.desktop_sort_by = FM_FOLDER_MODEL_COL_MTIME;
#else
    cfg->sort_type = GTK_SORT_ASCENDING;
    cfg->sort_by = COL_FILE_NAME;
    cfg->desktop_section.desktop_sort_type = GTK_SORT_ASCENDING;
    cfg->desktop_section.desktop_sort_by = COL_FILE_MTIME;
#endif
    cfg->desktop_section.wallpaper_common = TRUE;
}


FmConfig *fm_app_config_new(void)
{
    return (FmConfig*)g_object_new(FM_APP_CONFIG_TYPE, NULL);
}

void fm_app_config_load_desktop_config(GKeyFile *kf, const char *group, FmDesktopConfig *cfg)
{
    char* tmp;
    int tmp_int;

    if (!g_key_file_has_group(kf, group))
        return;

    cfg->configured = TRUE;
    if(fm_key_file_get_int(kf, group, "wallpaper_mode", &tmp_int))
        cfg->wallpaper_mode = (FmWallpaperMode)tmp_int;

    if(cfg->wallpapers_configured > 0)
    {
        int i;

        for(i = 0; i < cfg->wallpapers_configured; i++)
            g_free(cfg->wallpapers[i]);
        g_free(cfg->wallpapers);
    }
    g_free(cfg->wallpaper);
    cfg->wallpaper = NULL;
    fm_key_file_get_int(kf, group, "wallpapers_configured", &cfg->wallpapers_configured);
    if(cfg->wallpapers_configured > 0)
    {
        char wpn_buf[32];
        int i;

        cfg->wallpapers = g_malloc(cfg->wallpapers_configured * sizeof(char *));
        for(i = 0; i < cfg->wallpapers_configured; i++)
        {
            snprintf(wpn_buf, sizeof(wpn_buf), "wallpaper%d", i);
            tmp = g_key_file_get_string(kf, group, wpn_buf, NULL);
            cfg->wallpapers[i] = tmp;
        }
    }
    fm_key_file_get_bool(kf, group, "wallpaper_common", &cfg->wallpaper_common);
    if (cfg->wallpaper_common)
    {
        tmp = g_key_file_get_string(kf, group, "wallpaper", NULL);
        g_free(cfg->wallpaper);
        cfg->wallpaper = tmp;
    }

    tmp = g_key_file_get_string(kf, group, "desktop_bg", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_bg);
        g_free(tmp);
    }
    tmp = g_key_file_get_string(kf, group, "desktop_fg", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_fg);
        g_free(tmp);
    }
    tmp = g_key_file_get_string(kf, group, "desktop_shadow", NULL);
    if(tmp)
    {
        gdk_color_parse(tmp, &cfg->desktop_shadow);
        g_free(tmp);
    }

    tmp = g_key_file_get_string(kf, group, "desktop_font", NULL);
    g_free(cfg->desktop_font);
    cfg->desktop_font = tmp;

    fm_key_file_get_bool(kf, group, "show_wm_menu", &cfg->show_wm_menu);
    _parse_sort(kf, group, &cfg->desktop_sort_type, &cfg->desktop_sort_by);
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

    /* [desktop] section */
    fm_app_config_load_desktop_config(kf, "desktop", &cfg->desktop_section);

    /* ui */
    fm_key_file_get_int(kf, "ui", "always_show_tabs", &cfg->always_show_tabs);
    fm_key_file_get_int(kf, "ui", "hide_close_btn", &cfg->hide_close_btn);
    fm_key_file_get_int(kf, "ui", "max_tab_chars", &cfg->max_tab_chars);

    fm_key_file_get_int(kf, "ui", "win_width", &cfg->win_width);
    fm_key_file_get_int(kf, "ui", "win_height", &cfg->win_height);

    fm_key_file_get_int(kf, "ui", "splitter_pos", &cfg->splitter_pos);

    if(fm_key_file_get_int(kf, "ui", "side_pane_mode", &tmp_int))
        cfg->side_pane_mode = (FmSidePaneMode)tmp_int;

    /* default values for folder views */
    if(!fm_key_file_get_int(kf, "ui", "view_mode", &tmp_int) ||
       !FM_STANDARD_VIEW_MODE_IS_VALID(tmp_int))
        cfg->view_mode = FM_FV_ICON_VIEW;
    else
        cfg->view_mode = tmp_int;
    fm_key_file_get_bool(kf, "ui", "show_hidden", &cfg->show_hidden);
    _parse_sort(kf, "ui", &cfg->sort_type, &cfg->sort_by);
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
}

void fm_app_config_save_desktop_config(GString *buf, const char *group, FmDesktopConfig *cfg)
{
    g_string_append_printf(buf, "[%s]\n"
                                "wallpaper_mode=%d\n", group, cfg->wallpaper_mode);
    g_string_append_printf(buf, "wallpaper_common=%d\n", cfg->wallpaper_common);
    if (cfg->wallpapers && cfg->wallpapers_configured > 0)
    {
        int i;

        g_string_append_printf(buf, "wallpapers_configured=%d\n", cfg->wallpapers_configured);
        for (i = 0; i < cfg->wallpapers_configured; i++)
            if (cfg->wallpapers[i])
                g_string_append_printf(buf, "wallpaper%d=%s\n", i, cfg->wallpapers[i]);
    }
    if (cfg->wallpaper_common && cfg->wallpaper)
        g_string_append_printf(buf, "wallpaper=%s\n", cfg->wallpaper);
    g_string_append_printf(buf, "desktop_bg=#%02x%02x%02x\n",
                           cfg->desktop_bg.red/257,
                           cfg->desktop_bg.green/257,
                           cfg->desktop_bg.blue/257);
    g_string_append_printf(buf, "desktop_fg=#%02x%02x%02x\n",
                           cfg->desktop_fg.red/257,
                           cfg->desktop_fg.green/257,
                           cfg->desktop_fg.blue/257);
    g_string_append_printf(buf, "desktop_shadow=#%02x%02x%02x\n",
                           cfg->desktop_shadow.red/257,
                           cfg->desktop_shadow.green/257,
                           cfg->desktop_shadow.blue/257);
    if(cfg->desktop_font && *cfg->desktop_font)
        g_string_append_printf(buf, "desktop_font=%s\n", cfg->desktop_font);
    g_string_append_printf(buf, "show_wm_menu=%d\n", cfg->show_wm_menu);
    _save_sort(buf, cfg->desktop_sort_type, cfg->desktop_sort_by);
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
        _save_sort(buf, cfg->sort_type, cfg->sort_by);

        path = g_build_filename(dir_path, "pcmanfm.conf", NULL);
        g_file_set_contents(path, buf->str, buf->len, NULL);
        g_string_free(buf, TRUE);
        g_free(path);
    }
    g_free(dir_path);
}
