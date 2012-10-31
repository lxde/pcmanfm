/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2009 Thomas H.P. Andersen <phomes@gmail.com>,
 *              2009 Javier Jardón <jjardon@gnome.org>
 *
 *  This runtime is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This runtime is distributed in the hope runtime it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this runtime; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GSEAL_GTK_COMPAT_H
#define GSEAL_GTK_COMPAT_H

G_BEGIN_DECLS

#if !GTK_CHECK_VERSION(2, 22, 0)
#  define gdk_drag_context_get_actions(drag_context) \
        drag_context->actions
#endif /* GTK < 2.22.0 */

#if !GTK_CHECK_VERSION(2, 21, 0)
#define GDK_KEY_BackSpace                       GDK_BackSpace
#define GDK_KEY_Tab                             GDK_Tab
#define GDK_KEY_ISO_Left_Tab                    GDK_ISO_Left_Tab
#define GDK_KEY_Page_Down                       GDK_Page_Down
#define GDK_KEY_Page_Up                         GDK_Page_Up
#define GDK_KEY_Escape                          GDK_Escape
#define GDK_KEY_Left                            GDK_Left
#define GDK_KEY_Right                           GDK_Right
#define GDK_KEY_Up                              GDK_Up
#define GDK_KEY_Down                            GDK_Down
#define GDK_KEY_space                           GDK_space
#define GDK_KEY_F2                              GDK_F2
#define GDK_KEY_Return                          GDK_Return
#define GDK_KEY_ISO_Enter                       GDK_ISO_Enter
#define GDK_KEY_KP_Enter                        GDK_KP_Enter
#endif

#if !GTK_CHECK_VERSION (2, 20, 0)
#define gtk_widget_get_mapped(widget)           GTK_WIDGET_MAPPED(widget)
#define gtk_widget_get_realized(widget)         GTK_WIDGET_REALIZED(widget)
#endif /* GTK < 2.20.0 */

#if !GTK_CHECK_VERSION (2, 18, 0)
#define gtk_widget_has_focus(widget)            GTK_WIDGET_HAS_FOCUS(widget)
#define gtk_widget_get_visible(widget)          GTK_WIDGET_VISIBLE(widget)
#endif /* GTK+ < 2.18.0 */


G_END_DECLS

#endif /* GSEAL_GTK_COMPAT_H */
/* ex:ts=8:noet: */
