/*
 *      pref.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <fm-config.h>

#include "pref.h"
#include "app-config.h"
#include "desktop.h"

#define INIT_BOOL(b, st, name)  init_bool(b, #name, G_STRUCT_OFFSET(st, name))
#define INIT_ICON_SIZES(b, name) init_icon_sizes(b, #name, G_STRUCT_OFFSET(FmConfig, name))
#define INIT_COMBO(b, st, name) init_combo(b, #name, G_STRUCT_OFFSET(st, name))

static GtkWidget* pref_dlg = NULL;
static GtkWidget* notebook = NULL;
/*
static GtkWidget* icon_size_combo[3] = {0};
static GtkWidget* bookmark_combo = NULL
static GtkWidget* use_trash;
*/

static void on_response(GtkDialog* dlg, int res, gpointer user_data)
{
    gtk_widget_destroy(dlg);
    pref_dlg = NULL;
}

static void on_icon_size_changed(GtkComboBox* combo, gpointer _off)
{
    GtkTreeIter it;
    if(gtk_combo_box_get_active_iter(combo, &it))
    {
        gsize off = GPOINTER_TO_SIZE(_off);
        int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
        int size;
        GtkTreeModel* model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get(model, &it, 1, &size, -1);
        if(size != *val)
        {
            const char* name = gtk_widget_get_name((GtkWidget*)combo);
            *val = size;
            fm_config_emit_changed(fm_config, name);
        }
    }
}

static void init_icon_sizes(GtkBuilder* builder, const char* name, gsize off)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)combo, name);
    gtk_tree_model_get_iter_first(model, &it);
    gtk_combo_box_set_active_iter(combo, &it);
    do{
        int size;
        gtk_tree_model_get(model, &it, 1, &size, -1);
        if(size == *val)
        {
            gtk_combo_box_set_active_iter(combo, &it);
            break;
        }
    }while(gtk_tree_model_iter_next(model, &it));
    g_signal_connect(combo, "changed", G_CALLBACK(on_icon_size_changed), GSIZE_TO_POINTER(off));
}

static void on_combo_changed(GtkComboBox* combo, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    int sel = gtk_combo_box_get_active(combo);
    if(sel != *val)
    {
        const char* name = gtk_widget_get_name((GtkWidget*)combo);
        *val = sel;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_combo(GtkBuilder* builder, const char* name, gsize off)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)combo, name);
    gtk_combo_box_set_active(combo, *val);
    g_signal_connect(combo, "changed", G_CALLBACK(on_combo_changed), GSIZE_TO_POINTER(off));
}

static void on_toggled(GtkToggleButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gboolean new_val = gtk_toggle_button_get_active(btn);
    if(*val != new_val)
    {
        const char* name = gtk_widget_get_name((GtkWidget*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_bool(GtkBuilder* b, const char* name, gsize off)
{
    GtkToggleButton* btn = GTK_TOGGLE_BUTTON(gtk_builder_get_object(b, name));
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)btn, name);
    gtk_toggle_button_set_active(btn, *val);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_toggled), GSIZE_TO_POINTER(off));
}

gboolean fm_edit_preference( GtkWindow* parent, int page )
{
    if(!pref_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();
        GtkWidget* item;
        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/pref.ui", NULL);
        pref_dlg = gtk_builder_get_object(builder, "dlg");
        notebook = gtk_builder_get_object(builder, "notebook");

        INIT_BOOL(builder, FmConfig, single_click);
        INIT_BOOL(builder, FmConfig, confirm_del);
        INIT_BOOL(builder, FmConfig, use_trash);
        INIT_BOOL(builder, FmConfig, show_thumbnail);
        INIT_BOOL(builder, FmConfig, si_unit);

        INIT_BOOL(builder, FmAppConfig, manage_desktop);
        INIT_BOOL(builder, FmAppConfig, always_show_tabs);
        INIT_BOOL(builder, FmAppConfig, hide_close_btn);

        INIT_COMBO(builder, FmAppConfig, bm_open_method);

        INIT_ICON_SIZES(builder, big_icon_size);
        INIT_ICON_SIZES(builder, small_icon_size);
        INIT_ICON_SIZES(builder, pane_icon_size);

        g_signal_connect(pref_dlg, "response", G_CALLBACK(on_response), NULL);
        g_object_unref(builder);
    }
    gtk_window_present(pref_dlg);
    gtk_notebook_set_current_page(notebook, page);
    return TRUE;
}
