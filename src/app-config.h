/*
 *      app-config.h
 *
 *      Copyright 2010 - 2011 PCMan <pcman.tw@gmail.com>
 *      Copyright 2012-2014 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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


#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <libfm/fm.h>
#include <libfm/fm-gtk.h>
#include <gtk/gtk.h>

#include "pcmanfm.h"

G_BEGIN_DECLS

#define app_config   ((FmAppConfig*)fm_config)

#define FM_APP_CONFIG_TYPE              (fm_app_config_get_type())
#define FM_APP_CONFIG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_APP_CONFIG_TYPE, FmAppConfig))
#define FM_APP_CONFIG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_APP_CONFIG_TYPE, FmAppConfigClass))
#define IS_FM_APP_CONFIG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_APP_CONFIG_TYPE))
#define IS_FM_APP_CONFIG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_APP_CONFIG_TYPE))

typedef enum
{
    FM_WP_COLOR,
    FM_WP_STRETCH,
    FM_WP_FIT,
    FM_WP_CENTER,
    FM_WP_TILE,
    FM_WP_CROP,
    FM_WP_SCREEN
}FmWallpaperMode;

typedef enum
{
    FM_OPEN_IN_CURRENT_TAB,
    FM_OPEN_IN_NEW_TAB,
    FM_OPEN_IN_NEW_WINDOW,
    FM_OPEN_IN_LAST_ACTIVE_WINDOW,
}FmOpenMethod;

typedef struct _FmAppConfig         FmAppConfig;
typedef struct _FmAppConfigClass        FmAppConfigClass;

typedef struct
{
    FmWallpaperMode wallpaper_mode;
    char* wallpaper;
    char** wallpapers;
    int wallpapers_configured;
    gboolean wallpaper_common;
    gint configured : 1;
    gint changed : 1;
    GdkColor desktop_bg;
    GdkColor desktop_fg;
    GdkColor desktop_shadow;
    char* desktop_font;
    char *folder; /* NULL if default, empty if no icons, else path */
    gboolean show_wm_menu;
#if FM_CHECK_VERSION(1, 0, 2)
    FmSortMode desktop_sort_type;
    FmFolderModelCol desktop_sort_by;
#else
    GtkSortType desktop_sort_type;
    int desktop_sort_by;
#endif
#if FM_CHECK_VERSION(1, 2, 0)
    gboolean show_documents;
    gboolean show_trash;
    gboolean show_mounts;
#endif
} FmDesktopConfig;

typedef struct
{
    char *last_used;
    gboolean dont_ask;
} FmAutorunChoice;

struct _FmAppConfig
{
    FmConfig parent;
    /* config */
    int bm_open_method;

    /* volume */
    gboolean mount_on_startup;
    gboolean mount_removable;
    gboolean autorun;
    GHashTable *autorun_choices;

    /* ui */
    gboolean always_show_tabs;
    gboolean hide_close_btn;
    int max_tab_chars;
    int win_width;
    int win_height;
    int splitter_pos;
    gboolean media_in_new_tab;
    gboolean desktop_folder_new_win;
    gboolean change_tab_on_drop;
    gboolean close_on_unmount;
#if FM_CHECK_VERSION(1, 2, 0)
    gboolean focus_previous;
#endif
    gboolean maximized;
    gboolean pathbar_mode_buttons;

    FmSidePaneMode side_pane_mode;

    /* default values for folder views */
    guint view_mode;
    gboolean show_hidden;
#if FM_CHECK_VERSION(1, 0, 2)
    FmSortMode sort_type;
    FmFolderModelCol sort_by;
    /* list of columns formatted as name[:size] */
    char **columns;
#else
    GtkSortType sort_type;
    int sort_by;
#endif
    gboolean show_statusbar;
#if FM_CHECK_VERSION(1, 2, 0)
    char *home_path;
#endif

    /*char* su_cmd;*/

    /* pre-1.2.0 style config - common settings for all monitors */
    FmDesktopConfig desktop_section;

    /* toolbar settings */
    struct {
        gboolean visible : 1;
        gboolean new_win : 1;
        gboolean new_tab : 1;
        gboolean nav : 1;
        gboolean home : 1;
    } tb;
};

struct _FmAppConfigClass
{
    FmConfigClass parent_class;
};

GType       fm_app_config_get_type      (void);
FmConfig*   fm_app_config_new           ();

void fm_app_config_load_from_profile(FmAppConfig* cfg, const char* name);

void fm_app_config_load_from_key_file(FmAppConfig* cfg, GKeyFile* kf);

void fm_app_config_save_profile(FmAppConfig* cfg, const char* name);

void fm_app_config_load_desktop_config(GKeyFile *kf, const char *group, FmDesktopConfig *cfg);
void fm_app_config_save_desktop_config(GString *buf, const char *group, FmDesktopConfig *cfg);

#if FM_CHECK_VERSION(1, 0, 2)
gboolean fm_app_config_get_config_for_path(FmPath *path, FmSortMode *mode,
                                           FmFolderModelCol *by,
                                           FmStandardViewMode *view_mode,
                                           gboolean *show_hidden,
                                           char ***columns);
void fm_app_config_save_config_for_path(FmPath *path, FmSortMode mode,
                                        FmFolderModelCol by,
                                        FmStandardViewMode view_mode,
                                        gboolean show_hidden,
                                        char **columns);
#else
gboolean fm_app_config_get_config_for_path(FmPath *path, GtkSortType *mode,
                                           gint *by, FmStandardViewMode *view_mode,
                                           gboolean *show_hidden, char ***columns);
void fm_app_config_save_config_for_path(FmPath *path, GtkSortType mode, gint by,
                                        FmStandardViewMode view_mode,
                                        gboolean show_hidden, char **columns);
#endif
void fm_app_config_clear_config_for_path(FmPath *path);

void fm_app_config_set_autorun_choice(FmAppConfig *cfg,
                                      const char *content_type,
                                      const char *app, gboolean dont_ask);

FmWallpaperMode fm_app_wallpaper_get_mode_by_name(const char *name);
const char *fm_app_wallpaper_get_mode_name(FmWallpaperMode mode);

G_END_DECLS

#endif /* __APP_CONFIG_H__ */
