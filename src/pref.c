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

#include "pcmanfm.h"

#include "pref.h"
#include "app-config.h"
#include "desktop.h"

#define INIT_BOOL(b, st, name, changed_notify)  init_bool(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_COMBO(b, st, name, changed_notify) init_combo(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_ICON_SIZES(b, name) init_icon_sizes(b, #name, G_STRUCT_OFFSET(FmConfig, name))
#define INIT_COLOR(b, st, name, changed_notify)  init_color(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)

static GtkWidget* pref_dlg = NULL;
static GtkWidget* notebook = NULL;
/*
static GtkWidget* icon_size_combo[3] = {0};
static GtkWidget* bookmark_combo = NULL
static GtkWidget* use_trash;
*/

static GtkWidget* desktop_pref_dlg = NULL;

static void on_response(GtkDialog* dlg, int res, GtkWidget** pdlg)
{
    gtk_widget_destroy(dlg);
    *pdlg = NULL;
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
        const char* name = g_object_get_data((GObject*)combo, "changed");
        if(!name)
            name = gtk_widget_get_name((GtkWidget*)combo);
        *val = sel;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_combo(GtkBuilder* builder, const char* name, gsize off, const char* changed_notify)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)combo, name);
    if(changed_notify)
        g_object_set_data_full(combo, "changed", g_strdup(changed_notify), g_free);
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
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_widget_get_name((GtkWidget*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_bool(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkToggleButton* btn = GTK_TOGGLE_BUTTON(gtk_builder_get_object(b, name));
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)btn, name);
    if(changed_notify)
        g_object_set_data_full(btn, "changed", g_strdup(changed_notify), g_free);
    gtk_toggle_button_set_active(btn, *val);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_toggled), GSIZE_TO_POINTER(off));
}

static void on_color_set(GtkColorButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    GdkColor* val = (GdkColor*)G_STRUCT_MEMBER_P(fm_config, off);
    GdkColor new_val;
    gtk_color_button_get_color(btn, &new_val);
    if( !gdk_color_equal(val, &new_val) )
    {
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_widget_get_name((GtkWidget*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_color(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkFontButton* btn = GTK_FONT_BUTTON(gtk_builder_get_object(b, name));
    GdkColor* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gtk_widget_set_name((GtkWidget*)btn, name);
    if(changed_notify)
        g_object_set_data_full(btn, "changed", g_strdup(changed_notify), g_free);
    gtk_color_button_set_color(btn, val);
    g_signal_connect(btn, "color-set", G_CALLBACK(on_color_set), GSIZE_TO_POINTER(off));
}

void fm_edit_preference( GtkWindow* parent, int page )
{
    if(!pref_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();
        GtkWidget* item;
        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/pref.ui", NULL);
        pref_dlg = gtk_builder_get_object(builder, "dlg");
        notebook = gtk_builder_get_object(builder, "notebook");

        INIT_BOOL(builder, FmConfig, single_click, NULL);
        INIT_BOOL(builder, FmConfig, confirm_del, NULL);
        INIT_BOOL(builder, FmConfig, use_trash, NULL);
        INIT_BOOL(builder, FmConfig, show_thumbnail, NULL);
        INIT_BOOL(builder, FmConfig, si_unit, NULL);

        INIT_BOOL(builder, FmAppConfig, always_show_tabs, NULL);
        INIT_BOOL(builder, FmAppConfig, hide_close_btn, NULL);

        INIT_COMBO(builder, FmAppConfig, bm_open_method, NULL);

        INIT_ICON_SIZES(builder, big_icon_size);
        INIT_ICON_SIZES(builder, small_icon_size);
        INIT_ICON_SIZES(builder, pane_icon_size);

        g_signal_connect(pref_dlg, "response", G_CALLBACK(on_response), &pref_dlg);
        g_object_unref(builder);

        pcmanfm_ref();
        g_signal_connect(pref_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL);
    }
    gtk_window_present(pref_dlg);
    gtk_notebook_set_current_page(notebook, page);
}

static void on_wallpaper_set(GtkFileChooserButton* btn, gpointer user_data)
{
    char* file = gtk_file_chooser_get_filename(btn);
    g_free(app_config->wallpaper);
    app_config->wallpaper = file;
    fm_config_emit_changed(fm_config, "wallpaper");
}

static void on_update_img_preview( GtkFileChooser *chooser, GtkImage* img )
{
    char* file = gtk_file_chooser_get_preview_filename( chooser );
    GdkPixbuf* pix = NULL;
    if( file )
    {
        pix = gdk_pixbuf_new_from_file_at_scale( file, 128, 128, TRUE, NULL );
        g_free( file );
    }
    if( pix )
    {
        gtk_file_chooser_set_preview_widget_active(chooser, TRUE);
        gtk_image_set_from_pixbuf( img, pix );
        g_object_unref( pix );
    }
    else
    {
        gtk_image_clear( img );
        gtk_file_chooser_set_preview_widget_active(chooser, FALSE);
    }
}

static void on_desktop_font_set(GtkFontButton* btn, gpointer user_data)
{
    const char* font = gtk_font_button_get_font_name(btn);
    if(font)
    {
        g_free(app_config->desktop_font);
        app_config->desktop_font = g_strdup(font);
        fm_config_emit_changed(fm_config, "desktop_font");
    }
}

void fm_desktop_preference()
{
    if(!desktop_pref_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();
        GtkWidget* item, *img_preview;
        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/desktop-pref.ui", NULL);
        desktop_pref_dlg = gtk_builder_get_object(builder, "dlg");

        item = gtk_builder_get_object(builder, "wallpaper");
        g_signal_connect(item, "file-set", G_CALLBACK(on_wallpaper_set), NULL);
        img_preview = gtk_image_new();
        gtk_misc_set_alignment(img_preview, 0.5, 0.0);
        gtk_widget_set_size_request( img_preview, 128, 128 );
        gtk_file_chooser_set_preview_widget( (GtkFileChooser*)item, img_preview );
        g_signal_connect( item, "update-preview", G_CALLBACK(on_update_img_preview), img_preview );
        gtk_file_chooser_set_filename(item, app_config->wallpaper);

        INIT_COMBO(builder, FmAppConfig, wallpaper_mode, "wallpaper");
        INIT_COLOR(builder, FmAppConfig, desktop_bg, "wallpaper");

        INIT_COLOR(builder, FmAppConfig, desktop_fg, "desktop_text");
        INIT_COLOR(builder, FmAppConfig, desktop_shadow, "desktop_text");

        item = gtk_builder_get_object(builder, "desktop_font");
        gtk_font_button_set_font_name(item, app_config->desktop_font);
        g_signal_connect(item, "font-set", G_CALLBACK(on_desktop_font_set), NULL);

        g_signal_connect(desktop_pref_dlg, "response", G_CALLBACK(on_response), &desktop_pref_dlg);
        g_object_unref(builder);

        pcmanfm_ref();
        g_signal_connect(desktop_pref_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL);
    }
    gtk_window_present(desktop_pref_dlg);
}

