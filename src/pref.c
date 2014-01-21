/*
 *      pref.c
 *
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libfm/fm.h>

#include "pcmanfm.h"

#include "pref.h"
#include "app-config.h"
#include "desktop.h"
#include "main-win.h"

#include <glib/gi18n.h>
#include <string.h>

#define INIT_BOOL(b, st, name, changed_notify)  init_bool(b, #name, G_STRUCT_OFFSET(st, name), changed_notify, FALSE)
#define INIT_BOOL_SHOW(b, st, name, changed_notify)  init_bool(b, #name, G_STRUCT_OFFSET(st, name), changed_notify, TRUE)
#define INIT_COMBO(b, st, name, changed_notify) init_combo(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_ICON_SIZES(b, name) init_icon_sizes(b, #name, G_STRUCT_OFFSET(FmConfig, name))
#define INIT_SPIN(b, st, name, changed_notify)  init_spin(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)
#define INIT_ENTRY(b, st, name, changed_notify)  init_entry(b, #name, G_STRUCT_OFFSET(st, name), changed_notify)

static GtkWindow* pref_dlg = NULL;
static GtkNotebook* notebook = NULL;
/*
static GtkWidget* icon_size_combo[3] = {0};
static GtkWidget* bookmark_combo = NULL
static GtkWidget* use_trash;
*/

static void on_response(GtkDialog* dlg, int res, GtkWindow** pdlg)
{
    *pdlg = NULL;
    pcmanfm_save_config(TRUE);
    gtk_widget_destroy(GTK_WIDGET(dlg));
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
            const char* name = gtk_buildable_get_name((GtkBuildable*)combo);
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
            name = gtk_buildable_get_name((GtkBuildable*)combo);
        *val = sel;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_combo(GtkBuilder* builder, const char* name, gsize off, const char* changed_notify)
{
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, name);
    int* val = (int*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(combo), "changed", g_strdup(changed_notify), g_free);
    gtk_combo_box_set_active(combo, *val);
    g_signal_connect(combo, "changed", G_CALLBACK(on_combo_changed), GSIZE_TO_POINTER(off));
}

static void on_archiver_combo_changed(GtkComboBox* combo, gpointer user_data)
{
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    GtkTreeIter it;
    if(gtk_combo_box_get_active_iter(combo, &it))
    {
        FmArchiver* archiver;
        gtk_tree_model_get(model, &it, 1, &archiver, -1);
        if(archiver)
        {
            g_free(fm_config->archiver);
            fm_config->archiver = g_strdup(archiver->program);
            fm_archiver_set_default(archiver);
            fm_config_emit_changed(fm_config, "archiver");
        }
    }
}

/* archiver integration */
static void init_archiver_combo(GtkBuilder* builder)
{
    GtkListStore* model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
    GtkComboBox* combo = (GtkComboBox*)gtk_builder_get_object(builder, "archiver");
    GtkTreeIter it;
    const GList* archivers = fm_archiver_get_all();
    FmArchiver* default_archiver = fm_archiver_get_default();
    const GList* l;

    gtk_combo_box_set_model(combo, GTK_TREE_MODEL(model));

    for(l = archivers; l; l=l->next)
    {
        FmArchiver* archiver = (FmArchiver*)l->data;
        gtk_list_store_insert_with_values(model, &it, -1,
                        0, archiver->program,
                        1, archiver, -1);
        if(archiver == default_archiver)
            gtk_combo_box_set_active_iter(combo, &it);
    }
    g_object_unref(model);
    g_signal_connect(combo, "changed", G_CALLBACK(on_archiver_combo_changed), NULL);
}

#if FM_CHECK_VERSION(1, 2, 0)
static void on_auto_sel_changed(GtkRange *scale, gpointer unused)
{
    gint new_val = gtk_range_get_value(scale) * 1000;

    if (new_val != fm_config->auto_selection_delay)
    {
        fm_config->auto_selection_delay = new_val;
        fm_config_emit_changed(fm_config, "auto_selection_delay");
    }
}

static void init_auto_selection_delay_scale(GtkBuilder* builder)
{
    GtkScale *scale;
    gdouble val = fm_config->auto_selection_delay * 0.001;

    scale = GTK_SCALE(gtk_builder_get_object(builder, "auto_selection_delay"));
    gtk_range_set_value(GTK_RANGE(scale), val);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_auto_sel_changed), NULL);
    gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "auto_sel_box")));
}

static void on_drop_default_action_changed(GtkComboBox* combo, gpointer smart_desktop_autodrop)
{
    int sel = gtk_combo_box_get_active(combo);

    /* translate FmDndDestDropAction <- GtkListStore index */
    switch (sel)
    {
    case 0:
        sel = FM_DND_DEST_DROP_AUTO;
        break;
    case 1:
        sel = FM_DND_DEST_DROP_COPY;
        break;
    case 2:
        sel = FM_DND_DEST_DROP_MOVE;
        break;
    default:
        sel = FM_DND_DEST_DROP_ASK;
    }
    if (sel != fm_config->drop_default_action)
    {
        fm_config->drop_default_action = sel;
        fm_config_emit_changed(fm_config, "drop_default_action");
        gtk_widget_set_sensitive(smart_desktop_autodrop, sel == FM_DND_DEST_DROP_AUTO);
    }
}

static void init_drop_default_action_combo(GtkBuilder* builder)
{
    GObject *combo = gtk_builder_get_object(builder, "drop_default_action");
    GObject *smart_desktop_autodrop = gtk_builder_get_object(builder, "smart_desktop_autodrop");
    gint var;

    /* translate FmDndDestDropAction -> GtkListStore index */
    switch (fm_config->drop_default_action)
    {
    case FM_DND_DEST_DROP_COPY:
        var = 1;
        break;
    case FM_DND_DEST_DROP_MOVE:
        var = 2;
        break;
    case FM_DND_DEST_DROP_ASK:
        var = 3;
        break;
    default:
        var = 0;
    }
    gtk_combo_box_set_active((GtkComboBox*)combo, var);
    gtk_widget_set_sensitive(GTK_WIDGET(smart_desktop_autodrop),
                             fm_config->drop_default_action == FM_DND_DEST_DROP_AUTO);
    g_signal_connect(combo, "changed", G_CALLBACK(on_drop_default_action_changed),
                     smart_desktop_autodrop);
}
#endif

static void on_toggled(GtkToggleButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    gboolean new_val = gtk_toggle_button_get_active(btn);
    if(*val != new_val)
    {
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_bool(GtkBuilder* b, const char* name, gsize off,
                      const char* changed_notify, gboolean force_show)
{
    GtkToggleButton* btn = GTK_TOGGLE_BUTTON(gtk_builder_get_object(b, name));
    gboolean* val = (gboolean*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    if(force_show)
        gtk_widget_show(GTK_WIDGET(btn));
    gtk_toggle_button_set_active(btn, *val);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_toggled), GSIZE_TO_POINTER(off));
}

static void on_single_click_toggled(GtkToggleButton* btn, gpointer auto_sel_box)
{
    gboolean new_val = gtk_toggle_button_get_active(btn);

    if (new_val != fm_config->single_click)
    {
        fm_config->single_click = new_val;
        fm_config_emit_changed(fm_config, "single_click");
    }
    if (auto_sel_box)
        gtk_widget_set_sensitive(auto_sel_box, new_val);
}

static void on_use_trash_toggled(GtkToggleButton* btn, gpointer vbox_trash)
{
    gboolean new_val = gtk_toggle_button_get_active(btn);

    if (new_val != fm_config->use_trash)
    {
        fm_config->use_trash = new_val;
        fm_config_emit_changed(fm_config, "use_trash");
    }
    if (vbox_trash)
        gtk_widget_set_sensitive(vbox_trash, new_val);
}

static void on_close_on_unmount_toggled(GtkToggleButton *btn, FmAppConfig *cfg)
{
    gboolean new_val = gtk_toggle_button_get_active(btn);

    if (new_val != cfg->close_on_unmount)
    {
        cfg->close_on_unmount = new_val;
        fm_config_emit_changed(fm_config, "close_on_unmount");
    }
}

static void on_spin_changed(GtkSpinButton* btn, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    guint* val = (guint*)G_STRUCT_MEMBER_P(fm_config, off);
    guint new_val = gtk_spin_button_get_value(btn);
    if(*val != new_val)
    {
        const char* name = g_object_get_data((GObject*)btn, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)btn);
        *val = new_val;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_spin(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkSpinButton* btn = GTK_SPIN_BUTTON(gtk_builder_get_object(b, name));
    guint* val = (guint*)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    gtk_spin_button_set_value(btn, *val);
    g_signal_connect(btn, "value-changed", G_CALLBACK(on_spin_changed), GSIZE_TO_POINTER(off));
}

static void on_entry_changed(GtkEntry* entry, gpointer _off)
{
    gsize off = GPOINTER_TO_SIZE(_off);
    gchar** val = (gchar**)G_STRUCT_MEMBER_P(fm_config, off);
    const char* new_val = gtk_entry_get_text(entry);
    if(g_strcmp0(*val, new_val))
    {
        const char* name = g_object_get_data((GObject*)entry, "changed");
        if(!name)
            name = gtk_buildable_get_name((GtkBuildable*)entry);
        g_free(*val);
        *val = *new_val ? g_strdup(new_val) : NULL;
        fm_config_emit_changed(fm_config, name);
    }
}

static void init_entry(GtkBuilder* b, const char* name, gsize off, const char* changed_notify)
{
    GtkEntry* btn = GTK_ENTRY(gtk_builder_get_object(b, name));
    gchar** val = (gchar**)G_STRUCT_MEMBER_P(fm_config, off);
    if(changed_notify)
        g_object_set_data_full(G_OBJECT(btn), "changed", g_strdup(changed_notify), g_free);
    if(*val)
        gtk_entry_set_text(btn, *val);
    g_signal_connect(btn, "changed", G_CALLBACK(on_entry_changed), GSIZE_TO_POINTER(off));
}

static void on_tab_label_list_sel_changed(GtkTreeSelection* tree_sel, gpointer user_data)
{
    GtkTreePath* tp;
    GtkTreeIter it;
    GtkTreeModel* model;
    int page;
    if (!gtk_tree_selection_get_selected(tree_sel, &model, &it))
    {
        g_warning("pref: on_tab_label_list_sel_changed() got no selection");
        return;
    }
    tp = gtk_tree_model_get_path(model, &it);
    page = gtk_tree_path_get_indices(tp)[0];
    if (gtk_notebook_get_current_page(notebook) != page)
        gtk_notebook_set_current_page(notebook, page);
    gtk_tree_path_free(tp);
}

static void on_notebook_page_changed(GtkNotebook *notebook, gpointer page,
                                     guint n, GtkTreeView *view)
{
    GtkTreePath *tp;

    /* g_debug("changed pref page: %u", n); */
    gtk_tree_view_get_cursor(view, &tp, NULL);
    if (tp == NULL || gtk_tree_path_get_indices(tp)[0] != (int)n)
    {
        if (tp)
            gtk_tree_path_free(tp);
        tp = gtk_tree_path_new_from_indices(n, -1);
        gtk_tree_view_set_cursor(view, tp, NULL, FALSE);
    }
    gtk_tree_path_free(tp);
}

static gboolean on_key_press(GtkWidget* w, GdkEventKey* evt, GtkNotebook *notebook)
{
    int modifier = (evt->state & gtk_accelerator_get_default_mod_mask());

    if (modifier == GDK_MOD1_MASK) /* Alt */
    {
        if(evt->keyval >= '1' && evt->keyval <= '9') /* Alt + 1 ~ 9, nth tab */
        {
            gtk_notebook_set_current_page(notebook, evt->keyval - '1');
            return TRUE;
        }
    }
    return FALSE;
}

static void on_autorun_toggled(GtkToggleButton* btn, GtkWidget *autorun_choices_area)
{
    gboolean new_val = gtk_toggle_button_get_active(btn);

    if (new_val != app_config->autorun)
    {
        app_config->autorun = new_val;
        fm_config_emit_changed(fm_config, "autorun");
    }
    if (autorun_choices_area)
        gtk_widget_set_sensitive(autorun_choices_area, new_val);
}

static void on_remove_autorun_choice_clicked(GtkButton *button, GtkTreeView *view)
{
    GtkTreeSelection *tree_sel = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(tree_sel, &model), *l;
    GtkTreeIter it;

    /* convert paths to references */
    for (l = rows; l; l = l->next)
    {
        GtkTreePath *tp = l->data;
        l->data = gtk_tree_row_reference_new(model, tp);
        gtk_tree_path_free(tp);
    }
    /* remove rows from model */
    for (l = rows; l; l = l->next)
    {
        if (gtk_tree_model_get_iter(model, &it, gtk_tree_row_reference_get_path(l->data)))
        {
            char *type;

            gtk_tree_model_get(model, &it, 2, &type, -1);
            if (type)
                g_hash_table_remove(app_config->autorun_choices, type);
            g_free(type);
            gtk_list_store_remove(GTK_LIST_STORE(model), &it);
        }
        else
            g_critical("autorun_choice not found in model");
        gtk_tree_row_reference_free(l->data);
    }
    g_list_free(rows);
}

static void on_choices_sel_changed(GtkTreeSelection *selection, GtkWidget *btn)
{
    gtk_widget_set_sensitive(btn, gtk_tree_selection_count_selected_rows(selection) > 0);
}

#if FM_CHECK_VERSION(1, 2, 0)
static void on_use_home_path_toggled(GtkToggleButton *btn, GtkWidget *home_path_custom)
{
    gboolean active = gtk_toggle_button_get_active(btn);

    gtk_widget_set_sensitive(home_path_custom, active);
}

static void on_use_home_path_toggled2(GtkToggleButton *btn, GtkEntry *home_path)
{
    if (!gtk_toggle_button_get_active(btn))
        gtk_entry_set_text(home_path, fm_get_home_dir());
}

static void on_home_path_changed(GtkEntry *home_path, FmAppConfig *cfg)
{
    const char *path = gtk_entry_get_text(home_path);

    if (path[0] && strcmp(path, fm_get_home_dir()) != 0)
    {
        if (g_strcmp0(path, cfg->home_path) == 0) /* not changed */
            return;
        g_free(cfg->home_path);
        cfg->home_path = g_strdup(path);
    }
    else
    {
        g_free(cfg->home_path);
        cfg->home_path = NULL;
    }
    fm_config_emit_changed(FM_CONFIG(cfg), "home_path");
}

static void on_home_path_current_clicked(GtkButton *button, GtkEntry *home_path)
{
    FmMainWin *win = fm_main_win_get_last_active();
    FmPath *cwd;
    char *path;

    if (win == NULL || win->folder_view == NULL)
        return; /* FIXME: print warning? */
    cwd = fm_folder_view_get_cwd(win->folder_view);
    path = fm_path_to_str(cwd);
    gtk_entry_set_text(home_path, path);
    g_free(path);
}

static void on_add_to_blacklist_clicked(GtkButton *button, GtkListStore *model)
{
    char *item = fm_get_user_input(NULL, _("Add to Modules Blacklist"),
                                   _("Enter a blacklisted module mask:"), NULL);
    int i;
    GtkTreeIter it;

    if (item == NULL || item[0] == '\0') /* cancelled or empty */
        return;
    /* add a row to list */
    gtk_list_store_append(model, &it);
    gtk_list_store_set(model, &it, 0, item, 1, TRUE, -1);
    /* rebuild the blacklist */
    i = fm_config->modules_blacklist ? g_strv_length(fm_config->modules_blacklist) : 0;
    fm_config->modules_blacklist = g_renew(char *, fm_config->modules_blacklist, i + 2);
    fm_config->modules_blacklist[i+1] = NULL;
    fm_config->modules_blacklist[i] = item;
}

static void on_remove_from_blacklist_clicked(GtkButton *button, GtkTreeView *view)
{
    GtkTreeSelection *tree_sel = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(tree_sel, &model), *l;
    char *item;
    char **bl;
    int i;
    GtkTreeIter it;

    /* convert paths to references */
    for (l = rows; l; l = l->next)
    {
        GtkTreePath *tp = l->data;
        l->data = gtk_tree_row_reference_new(model, tp);
        gtk_tree_path_free(tp);
    }
    /* remove rows from model */
    for (l = rows; l; l = l->next)
    {
        if (gtk_tree_model_get_iter(model, &it, gtk_tree_row_reference_get_path(l->data)))
            gtk_list_store_remove(GTK_LIST_STORE(model), &it);
        else
            g_critical("the item not found in model");
        gtk_tree_row_reference_free(l->data);
    }
    g_list_free(rows);
    /* rebuild the blacklist */
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        bl = g_new(char *, gtk_tree_model_iter_n_children(model, NULL) + 1);
        i = 0;
        do
        {
            gtk_tree_model_get(model, &it, 0, &item, -1);
            bl[i++] = item;
        } while (gtk_tree_model_iter_next(model, &it));
        bl[i] = NULL;
    }
    else
        bl = NULL;
    g_strfreev(fm_config->modules_blacklist);
    fm_config->modules_blacklist = bl;
}

static void on_blacklist_sel_changed(GtkTreeSelection *selection, GtkWidget *btn)
{
    GList *rows, *l;
    GtkTreeModel *model;
    GtkTreePath *tp;
    GtkTreeIter it;
    gboolean can_del;

    if (gtk_tree_selection_count_selected_rows(selection) == 0)
    {
        gtk_widget_set_sensitive(btn, FALSE);
        return;
    }
    rows = gtk_tree_selection_get_selected_rows(selection, &model);
    can_del = TRUE;
    for (l = rows; l; l = l->next)
    {
        tp = l->data;
        if (can_del && gtk_tree_model_get_iter(model, &it, tp))
            gtk_tree_model_get(model, &it, 1, &can_del, -1);
        else
            can_del = FALSE;
        gtk_tree_path_free(tp);
    }
    g_list_free(rows);
    gtk_widget_set_sensitive(btn, can_del);
}

static void on_add_to_whitelist_clicked(GtkButton *button, GtkListStore *model)
{
    char *item = fm_get_user_input(NULL, _("Add to Modules Whitelist"),
                                   _("Enter a whitelisted module mask:"), NULL);
    int i;
    GtkTreeIter it;

    if (item == NULL || item[0] == '\0') /* cancelled or empty */
        return;
    /* add a row to list */
    gtk_list_store_append(model, &it);
    gtk_list_store_set(model, &it, 0, item, -1);
    /* rebuild the blacklist */
    i = fm_config->modules_whitelist ? g_strv_length(fm_config->modules_whitelist) : 0;
    fm_config->modules_whitelist = g_renew(char *, fm_config->modules_whitelist, i + 2);
    fm_config->modules_whitelist[i+1] = NULL;
    fm_config->modules_whitelist[i] = item;
}

static void on_remove_from_whitelist_clicked(GtkButton *button, GtkTreeView *view)
{
    GtkTreeSelection *tree_sel = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(tree_sel, &model), *l;
    char *item;
    char **wl;
    int i;
    GtkTreeIter it;

    /* convert paths to references */
    for (l = rows; l; l = l->next)
    {
        GtkTreePath *tp = l->data;
        l->data = gtk_tree_row_reference_new(model, tp);
        gtk_tree_path_free(tp);
    }
    /* remove rows from model */
    for (l = rows; l; l = l->next)
    {
        if (gtk_tree_model_get_iter(model, &it, gtk_tree_row_reference_get_path(l->data)))
            gtk_list_store_remove(GTK_LIST_STORE(model), &it);
        else
            g_critical("the item not found in model");
        gtk_tree_row_reference_free(l->data);
    }
    g_list_free(rows);
    /* rebuild the blacklist */
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        wl = g_new(char *, gtk_tree_model_iter_n_children(model, NULL) + 1);
        i = 0;
        do
        {
            gtk_tree_model_get(model, &it, 0, &item, -1);
            wl[i++] = item;
        } while (gtk_tree_model_iter_next(model, &it));
        wl[i] = NULL;
    }
    else
        wl = NULL;
    g_strfreev(fm_config->modules_whitelist);
    fm_config->modules_whitelist = wl;
}

static void on_whitelist_sel_changed(GtkTreeSelection *selection, GtkWidget *btn)
{
    gtk_widget_set_sensitive(btn, gtk_tree_selection_count_selected_rows(selection) > 0);
}
#endif

void fm_edit_preference( GtkWindow* parent, int page )
{
    if(!pref_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();
        GtkTreeView* tab_label_list;
        GtkTreeSelection* tree_sel;
        GtkListStore* tab_label_model;
        GObject *obj;
        GtkTreeIter it;
        int i, n;

        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/pref.ui", NULL);
        pref_dlg = GTK_WINDOW(gtk_builder_get_object(builder, "dlg"));
        notebook = GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook"));

        /* General tab */
        /* special handling for single_click */
        g_signal_connect(gtk_builder_get_object(builder, "single_click"),
                         "toggled", G_CALLBACK(on_single_click_toggled),
                         gtk_builder_get_object(builder, "auto_sel_box"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "single_click")),
                                     fm_config->single_click);
#if FM_CHECK_VERSION(1, 2, 0)
        init_auto_selection_delay_scale(builder);
#endif
        INIT_BOOL(builder, FmConfig, confirm_del, NULL);
        /* special handling for use_trash */
        g_signal_connect(gtk_builder_get_object(builder, "use_trash"),
                         "toggled", G_CALLBACK(on_use_trash_toggled),
                         gtk_builder_get_object(builder, "vbox_trash"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "use_trash")),
                                     fm_config->use_trash);
#if FM_CHECK_VERSION(1, 0, 2)
        INIT_BOOL_SHOW(builder, FmConfig, no_usb_trash, NULL);
#endif
#if FM_CHECK_VERSION(1, 2, 0)
        INIT_BOOL_SHOW(builder, FmConfig, confirm_trash, NULL);
#endif

#if FM_CHECK_VERSION(1, 2, 0)
        INIT_BOOL_SHOW(builder, FmConfig, quick_exec, NULL);
#endif

        INIT_COMBO(builder, FmAppConfig, bm_open_method, NULL);
#if FM_CHECK_VERSION(1, 2, 0)
        init_drop_default_action_combo(builder);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "drop_default_action")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "drop_default_action_label")));
        INIT_BOOL_SHOW(builder, FmConfig, smart_desktop_autodrop, NULL);
        INIT_BOOL_SHOW(builder, FmAppConfig, focus_previous, NULL);
#endif
        INIT_BOOL_SHOW(builder, FmAppConfig, change_tab_on_drop, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "on_unmount_vbox")));
        if (app_config->close_on_unmount)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "close_on_unmount")), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "go_home_on_unmount")), TRUE);
        g_signal_connect(gtk_builder_get_object(builder, "close_on_unmount"),
                         "toggled", G_CALLBACK(on_close_on_unmount_toggled), app_config);
        INIT_COMBO(builder, FmAppConfig, view_mode, NULL);
        /* FIXME: translate FmStandardViewMode <-> GtkListStore index */

        /* 'Display' tab */
        INIT_ICON_SIZES(builder, big_icon_size);
        INIT_ICON_SIZES(builder, small_icon_size);
        INIT_ICON_SIZES(builder, thumbnail_size);
        INIT_ICON_SIZES(builder, pane_icon_size);

        INIT_BOOL(builder, FmConfig, show_thumbnail, NULL);
        INIT_BOOL(builder, FmConfig, thumbnail_local, NULL);
        INIT_SPIN(builder, FmConfig, thumbnail_max, NULL);

        INIT_BOOL(builder, FmConfig, si_unit, NULL);
        INIT_BOOL(builder, FmConfig, backup_as_hidden, NULL);
#if FM_CHECK_VERSION(1, 2, 0)
        INIT_BOOL_SHOW(builder, FmConfig, show_full_names, NULL);
        INIT_BOOL_SHOW(builder, FmConfig, shadow_hidden, NULL);
#endif

        /* 'Layout' tab */
        INIT_BOOL(builder, FmAppConfig, hide_close_btn, NULL);
        INIT_BOOL(builder, FmAppConfig, always_show_tabs, NULL);
        INIT_SPIN(builder, FmAppConfig, max_tab_chars, NULL);

#if FM_CHECK_VERSION(1, 0, 2)
        INIT_BOOL(builder, FmConfig, no_child_non_expandable, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "vbox_dir_tree")));
#endif

#if FM_CHECK_VERSION(1, 2, 0)
        INIT_BOOL(builder, FmConfig, places_home, NULL);
        INIT_BOOL(builder, FmConfig, places_desktop, NULL);
        INIT_BOOL(builder, FmConfig, places_applications, NULL);
        INIT_BOOL(builder, FmConfig, places_trash, NULL);
        INIT_BOOL(builder, FmConfig, places_root, NULL);
        INIT_BOOL(builder, FmConfig, places_computer, NULL);
        INIT_BOOL(builder, FmConfig, places_network, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "vbox_places")));
#endif

        /* 'Volume management' tab */
        INIT_BOOL(builder, FmAppConfig, mount_on_startup, NULL);
        INIT_BOOL(builder, FmAppConfig, mount_removable, NULL);
        obj = gtk_builder_get_object(builder, "autorun");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), app_config->autorun);
        g_signal_connect(obj, "toggled", G_CALLBACK(on_autorun_toggled),
                         gtk_builder_get_object(builder, "autorun_choices_area"));
        INIT_BOOL(builder, FmAppConfig, media_in_new_tab, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "media_in_new_tab")));
        //INIT_BOOL(builder, FmAppConfig, close_on_unmount, NULL);

        /* autorun choices editing area */
        obj = gtk_builder_get_object(builder, "autorun_choices_area");
        if (obj)
        {
            GtkTreeView *view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "autorun_choices"));
            GtkListStore *model = gtk_list_store_new(3, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING);
            GtkTreeSelection *tree_sel;
            GtkTreeViewColumn *col;
            GtkCellRenderer *render;
            GIcon *icon;
            char *desc;
            GList *keys, *l;
            GtkTreeIter it;

            gtk_widget_set_sensitive(GTK_WIDGET(obj), app_config->autorun);
            gtk_widget_show(GTK_WIDGET(obj));

            tree_sel = gtk_tree_view_get_selection(view);
            gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_MULTIPLE);

            col = gtk_tree_view_column_new();
            render = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start(col, render, FALSE);
            gtk_tree_view_column_set_attributes(col, render, "gicon", 0, NULL);
            render = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(col, render, FALSE);
            gtk_tree_view_column_set_attributes(col, render, "text", 1, NULL);
            gtk_tree_view_append_column(view, col);
            keys = g_hash_table_get_keys(app_config->autorun_choices);
            for (l = keys; l; l = l->next)
            {
                gtk_list_store_append(model, &it);
                desc = g_content_type_get_description(l->data);
                icon = g_content_type_get_icon(l->data);
                gtk_list_store_set(model, &it, 0, icon, 1, desc, 2, l->data, -1);
                if (icon)
                    g_object_unref(icon);
                g_free(desc);
            }
            g_list_free(keys);
            gtk_tree_view_set_model(view, GTK_TREE_MODEL(model));
            /* handle 'Remove selected' button */
            obj = gtk_builder_get_object(builder, "remove_autorun_choice");
            g_signal_connect(obj, "clicked", G_CALLBACK(on_remove_autorun_choice_clicked),
                             view);
            /* update button sensitivity by tree selection */
            g_signal_connect(tree_sel, "changed", G_CALLBACK(on_choices_sel_changed),
                             obj);
        }

        /* 'Advanced' tab */
        INIT_ENTRY(builder, FmConfig, terminal, NULL);
        /*INIT_ENTRY(builder, FmAppConfig, su_cmd, NULL);*/
#if FM_CHECK_VERSION(1, 2, 0)
        INIT_ENTRY(builder, FmConfig, format_cmd, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "hbox_format")));
#endif

        /* archiver integration */
        init_archiver_combo(builder);

#if FM_CHECK_VERSION(1, 2, 0)
        INIT_BOOL(builder, FmConfig, only_user_templates, NULL);
        INIT_BOOL(builder, FmConfig, template_type_once, NULL);
        INIT_BOOL(builder, FmConfig, template_run_app, NULL);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(builder, "vbox_templates")));
        INIT_BOOL_SHOW(builder, FmConfig, defer_content_test, NULL);
#endif
        INIT_BOOL(builder, FmConfig, force_startup_notify, NULL);

        /* initialize the left side list used for switching among tabs */
        tab_label_list = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tab_label_list"));
        tab_label_model = gtk_list_store_new(1, G_TYPE_STRING);
        n = gtk_notebook_get_n_pages(notebook);
        for(i = 0; i < n; ++i)
        {
            /* this can be less efficient than iterating over a GList obtained by gtk_container_get_children().
            * However, the order of pages does matter here. So we use get_nth_page. */
            GtkWidget* page = gtk_notebook_get_nth_page(notebook, i);
            const char* title = gtk_notebook_get_tab_label_text(notebook, page);
            gtk_list_store_insert_with_values(tab_label_model, NULL, i, 0, title, -1);
        }
        gtk_tree_view_set_model(tab_label_list, GTK_TREE_MODEL(tab_label_model));
        gtk_tree_view_set_enable_search(tab_label_list, FALSE);
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tab_label_model), &it);
        tree_sel = gtk_tree_view_get_selection(tab_label_list);
        gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_BROWSE);
        gtk_tree_selection_select_iter(tree_sel, &it);
        g_object_unref(tab_label_model);
#if FM_CHECK_VERSION(1, 2, 0)
        obj = gtk_builder_get_object(builder, "home_path_vbox");
        if (obj)
        {
            gboolean is_set;

            gtk_widget_show(GTK_WIDGET(obj));
            obj = gtk_builder_get_object(builder, "use_home_path");
            g_signal_connect(obj, "toggled", G_CALLBACK(on_use_home_path_toggled),
                             gtk_builder_get_object(builder, "home_path_custom"));
            g_signal_connect(obj, "toggled", G_CALLBACK(on_use_home_path_toggled2),
                             gtk_builder_get_object(builder, "home_path"));
            is_set = app_config->home_path &&
                     strcmp(app_config->home_path, fm_get_home_dir()) != 0;
            if (is_set)
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(obj), TRUE);
            else
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "use_home_default")), TRUE);
            obj = gtk_builder_get_object(builder, "home_path");
            if (is_set)
                gtk_entry_set_text(GTK_ENTRY(obj), app_config->home_path);
            else
                gtk_entry_set_text(GTK_ENTRY(obj), fm_get_home_dir());
            g_signal_connect(obj, "changed", G_CALLBACK(on_home_path_changed), app_config);
            obj = gtk_builder_get_object(builder, "home_path_current");
            if (parent && IS_FM_MAIN_WIN(parent)) /* is called from menu */
                g_signal_connect(obj, "clicked", G_CALLBACK(on_home_path_current_clicked),
                                 gtk_builder_get_object(builder, "home_path"));
            else
                gtk_widget_set_sensitive(GTK_WIDGET(obj), FALSE);
        }
        /* modules blacklist and whitelist */
        obj = gtk_builder_get_object(builder, "modules_tab");
        if (obj)
        {
            GtkTreeView *view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "modules_blacklist"));
            GtkListStore *model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_BOOLEAN);
            GtkTreeSelection *tree_sel;
            GtkTreeViewColumn *col;
            GtkCellRenderer *render;
            char **item;
            GtkTreeIter it;

            gtk_widget_show(GTK_WIDGET(obj));

            /* blacklist */
            tree_sel = gtk_tree_view_get_selection(view);
            gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_MULTIPLE);

            col = gtk_tree_view_column_new();
            render = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(col, render, FALSE);
            gtk_tree_view_column_set_attributes(col, render, "text", 0, NULL);
            gtk_tree_view_append_column(view, col);
            for (item = fm_config->system_modules_blacklist; item && *item; item++)
            {
                gtk_list_store_append(model, &it);
                gtk_list_store_set(model, &it, 0, *item, 1, FALSE, -1);
            }
            for (item = fm_config->modules_blacklist; item && *item; item++)
            {
                gtk_list_store_append(model, &it);
                gtk_list_store_set(model, &it, 0, *item, 1, TRUE, -1);
            }
            gtk_tree_view_set_model(view, GTK_TREE_MODEL(model));
            /* handle 'Add' button */
            obj = gtk_builder_get_object(builder, "add_to_blacklist");
            g_signal_connect(obj, "clicked", G_CALLBACK(on_add_to_blacklist_clicked),
                             model);
            /* handle 'Remove' button */
            obj = gtk_builder_get_object(builder, "remove_from_blacklist");
            g_signal_connect(obj, "clicked", G_CALLBACK(on_remove_from_blacklist_clicked),
                             view);
            /* update button sensitivity by tree selection */
            g_signal_connect(tree_sel, "changed", G_CALLBACK(on_blacklist_sel_changed),
                             obj);

            /* whitelist */
            view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "modules_whitelist"));
            model = gtk_list_store_new(1, G_TYPE_STRING);
            tree_sel = gtk_tree_view_get_selection(view);
            gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_MULTIPLE);

            col = gtk_tree_view_column_new();
            render = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(col, render, FALSE);
            gtk_tree_view_column_set_attributes(col, render, "text", 0, NULL);
            gtk_tree_view_append_column(view, col);
            for (item = fm_config->modules_whitelist; item && *item; item++)
            {
                gtk_list_store_append(model, &it);
                gtk_list_store_set(model, &it, 0, *item, -1);
            }
            gtk_tree_view_set_model(view, GTK_TREE_MODEL(model));
            /* handle 'Add' button */
            obj = gtk_builder_get_object(builder, "add_to_whitelist");
            g_signal_connect(obj, "clicked", G_CALLBACK(on_add_to_whitelist_clicked),
                             model);
            /* handle 'Remove' button */
            obj = gtk_builder_get_object(builder, "remove_from_whitelist");
            g_signal_connect(obj, "clicked", G_CALLBACK(on_remove_from_whitelist_clicked),
                             view);
            /* update button sensitivity by tree selection */
            g_signal_connect(tree_sel, "changed", G_CALLBACK(on_whitelist_sel_changed),
                             obj);
        }
#endif
        g_signal_connect(tree_sel, "changed", G_CALLBACK(on_tab_label_list_sel_changed), notebook);
        g_signal_connect(notebook, "switch-page", G_CALLBACK(on_notebook_page_changed), tab_label_list);
        gtk_notebook_set_show_tabs(notebook, FALSE);

        g_signal_connect(pref_dlg, "response", G_CALLBACK(on_response), &pref_dlg);
        g_signal_connect(pref_dlg, "key-press-event", G_CALLBACK(on_key_press), notebook);
        g_object_unref(builder);

        pcmanfm_ref();
        g_signal_connect(pref_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL);
        if(parent)
            gtk_window_set_transient_for(pref_dlg, parent);
    }
    if(page < 0 || page >= gtk_notebook_get_n_pages(notebook))
        page = 0;
    gtk_notebook_set_current_page(notebook, page);
    gtk_window_present(pref_dlg);
}
