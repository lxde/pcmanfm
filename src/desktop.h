/*
 *      desktop.h
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


#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include <gtk/gtk.h>
#include <fm-gtk.h>

G_BEGIN_DECLS

#define FM_TYPE_DESKTOP				(fm_desktop_get_type())
#define FM_DESKTOP(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			FM_TYPE_DESKTOP, FmDesktop))
#define FM_DESKTOP_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			FM_TYPE_DESKTOP, FmDesktopClass))
#define FM_IS_DESKTOP(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			FM_TYPE_DESKTOP))
#define FM_IS_DESKTOP_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			FM_TYPE_DESKTOP))

typedef struct _FmDesktop			FmDesktop;
typedef struct _FmDesktopClass		FmDesktopClass;
typedef struct _FmDesktopItem       FmDesktopItem;

struct _FmDesktop
{
	GtkWindow parent;
    GdkGC* gc;
    PangoFontDescription* font;
    PangoLayout* pl;
    GtkCellRendererPixbuf* icon_render;
    GList* items;
    guint xpad;
    guint ypad;
    guint spacing;
    guint xmargin;
    guint ymargin;
    guint text_h;
    guint text_w;
    guint cell_w;
    guint cell_h;
    GdkRectangle working_area;
    FmDesktopItem* focus;
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
};

struct _FmDesktopClass
{
	GtkWindowClass parent_class;
};

/*
void fm_desktop_set_text_color(FmDesktop* desktop, GdkColor* clr);
void fm_desktop_set_bg_color(FmDesktop* desktop, GdkColor* clr);
// void fm_desktop_set_background(FmDesktop* desktop, );

void fm_desktop_win_set_icon_size(FmDesktop* desktop, guint size);
//void fm_desktop_win_set_show_thumbnails(FmDesktop* desktop, gboolean show);

// void fm_desktop_win_sort_items( FmDesktopWin* win, DWSortType sort_by, GtkSortType sort_type );

void fm_desktop_set_single_click(FmDesktop* desktop, gboolean single_click);
void fm_desktop_set_single_click_timeout(FmDesktop* desktop, guint timeout);
*/

GType		fm_desktop_get_type		(void);
GtkWidget*	fm_desktop_new			(void);

void fm_desktop_manager_init();
void fm_desktop_manager_finalize();

G_END_DECLS

#endif /* __DESKTOP_H__ */
