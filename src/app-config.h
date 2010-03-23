/*
 *      app-config.h
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


#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#include <libfm/fm.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define app_config   ((FmAppConfig*)fm_config)

#define FM_APP_CONFIG_TYPE				(fm_app_config_get_type())
#define FM_APP_CONFIG(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_APP_CONFIG_TYPE, FmAppConfig))
#define FM_APP_CONFIG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_APP_CONFIG_TYPE, FmAppConfigClass))
#define IS_FM_APP_CONFIG(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_APP_CONFIG_TYPE))
#define IS_FM_APP_CONFIG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_APP_CONFIG_TYPE))

typedef enum
{
    FM_WP_COLOR,
    FM_WP_STRETCH,
    FM_WP_FIT,
    FM_WP_CENTER,
    FM_WP_TILE
}FmWallpaperMode;

typedef struct _FmAppConfig			FmAppConfig;
typedef struct _FmAppConfigClass		FmAppConfigClass;

struct _FmAppConfig
{
	FmConfig parent;
    /* config */
    guint bm_open_method;

    /* volume */
    gboolean mount_on_startup;
    gboolean mount_removable;
    gboolean autorun;

    /* ui */
    gboolean always_show_tabs;
    gboolean hide_close_btn;
    int win_width;
    int win_height;
    int splitter_pos;

    /* default values for folder views */
    guint view_mode;
    gboolean show_hidden;
    GtkSortType sort_type;
    int sort_by;

    /* desktop manager */
    /* emit "changed::wallpaper" */
    FmWallpaperMode wallpaper_mode;
    char* wallpaper;
    GdkColor desktop_bg;
    /* emit "changed::desktop_text" */
    GdkColor desktop_fg;
    GdkColor desktop_shadow;
    /* emit "changed::desktop_font" */
    char* desktop_font;

    gboolean show_wm_menu;

    char* su_cmd;
};

struct _FmAppConfigClass
{
	FmConfigClass parent_class;
};

GType		fm_app_config_get_type		(void);
FmConfig*	fm_app_config_new			();

void fm_app_config_load_from_file(FmAppConfig* cfg, const char* name);

void fm_app_config_load_from_key_file(FmAppConfig* cfg, GKeyFile* kf);

void fm_app_config_save(FmAppConfig* cfg, const char* name);


G_END_DECLS

#endif /* __APP_CONFIG_H__ */
