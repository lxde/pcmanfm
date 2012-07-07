/*
 *      desktop.h
 *
 *      Copyright 2010 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include <gtk/gtk.h>
#include <libfm/fm-gtk.h>

G_BEGIN_DECLS

#define FM_TYPE_DESKTOP             (fm_desktop_get_type())
#define FM_DESKTOP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_DESKTOP, FmDesktop))
#define FM_DESKTOP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_DESKTOP, FmDesktopClass))
#define FM_IS_DESKTOP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_DESKTOP))
#define FM_IS_DESKTOP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_DESKTOP))

typedef struct _FmDesktop           FmDesktop;
typedef struct _FmDesktopClass      FmDesktopClass;
typedef struct _FmDesktopItem       FmDesktopItem;
typedef struct _FmBackgroundCache   FmBackgroundCache;

struct _FmDesktop
{
    GtkWindow parent;
    GdkGC* gc; /* FIXME: obsoleted, replace with Cairo */
    PangoLayout* pl;
    FmCellRendererPixbuf* icon_render;
    GList* fixed_items;
    guint xpad;
    guint ypad;
    guint spacing;
    guint xmargin;
    guint ymargin;
    guint text_h;
    guint text_w;
    guint pango_text_h;
    guint pango_text_w;
    guint cell_w;
    guint cell_h;
    GdkRectangle working_area;
    FmDesktopItem* focus;
    FmDesktopItem* drop_hilight;
    FmDesktopItem* hover_item;
    gint rubber_bending_x;
    gint rubber_bending_y;
    gint drag_start_x;
    gint drag_start_y;
    gboolean rubber_bending : 1;
    gboolean button_pressed : 1;
    gboolean dragging : 1;
    guint idle_layout;
    FmDndSrc* dnd_src;
    FmDndDest* dnd_dest;
    guint single_click_timeout_handler;
    FmFolderModel* model;
    FmBackgroundCache* wallpapers;
    GtkMenu* popup;
    uint cur_desktop;
};

struct _FmDesktopClass
{
    GtkWindowClass parent_class;
};

gboolean fm_desktop_has_selected_item(FmDesktop* desktop);
FmFileInfoList* fm_desktop_dup_selected_files(FmDesktop* desktop);
FmPathList* fm_desktop_dup_selected_paths(FmDesktop* desktop);

GType       fm_desktop_get_type     (void);
FmDesktop*  fm_desktop_new          (void);

FmDesktop*  fm_desktop_get          (guint screen);

void fm_desktop_manager_init();
void fm_desktop_manager_finalize();

G_END_DECLS

#endif /* __DESKTOP_H__ */
