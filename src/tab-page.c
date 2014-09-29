//      fm-tab-page.c
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//      Copyright 2012-2014 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <libfm/fm-gtk.h>
#include <glib/gi18n.h>

#include "app-config.h"
#include "main-win.h"
#include "tab-page.h"

#include "gseal-gtk-compat.h"

#include <stdlib.h>
#include <fnmatch.h>

/* Additional entries for FmFileMenu popup */
/* it is also used for FmSidePane context menu popup */
static const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<menuitem action='Term'/>"
    /* "<menuitem action='Search'/>" */
  "</placeholder>"
"</popup>";

static void on_open_in_new_tab(GtkAction* act, FmMainWin* win);
static void on_open_in_new_win(GtkAction* act, FmMainWin* win);
static void on_open_folder_in_terminal(GtkAction* act, FmMainWin* win);

/* Action entries for popup menu entries above */
static GtkActionEntry folder_menu_actions[]=
{
    {"NewTab", GTK_STOCK_NEW, N_("Open in New T_ab"), NULL, NULL, G_CALLBACK(on_open_in_new_tab)},
    {"NewWin", GTK_STOCK_NEW, N_("Open in New Win_dow"), NULL, NULL, G_CALLBACK(on_open_in_new_win)},
    {"Search", GTK_STOCK_FIND, NULL, NULL, NULL, NULL},
    {"Term", "utilities-terminal", N_("Open in Termina_l"), NULL, NULL, G_CALLBACK(on_open_folder_in_terminal)},
};

#define GET_MAIN_WIN(page)   FM_MAIN_WIN(gtk_widget_get_toplevel(GTK_WIDGET(page)))

enum {
    CHDIR,
    OPEN_DIR,
    STATUS,
    GOT_FOCUS,
    LOADED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static void fm_tab_page_finalize(GObject *object);
static void fm_tab_page_chdir_without_history(FmTabPage* page, FmPath* path);
static void on_folder_fs_info(FmFolder* folder, FmTabPage* page);
static void on_folder_start_loading(FmFolder* folder, FmTabPage* page);
static void on_folder_finish_loading(FmFolder* folder, FmTabPage* page);
static void on_folder_removed(FmFolder* folder, FmTabPage* page);
static void on_folder_unmount(FmFolder* folder, FmTabPage* page);
static void on_folder_content_changed(FmFolder* folder, FmTabPage* page);
static FmJobErrorAction on_folder_error(FmFolder* folder, GError* err, FmJobErrorSeverity severity, FmTabPage* page);

static void on_folder_view_sel_changed(FmFolderView* fv, gint n_sel, FmTabPage* page);
#if FM_CHECK_VERSION(1, 2, 0)
static void  on_folder_view_columns_changed(FmFolderView *fv, FmTabPage *page);
#endif
static gboolean on_folder_view_focus_in(GtkWidget *widget, GdkEvent *event, FmTabPage *page);
static char* format_status_text(FmTabPage* page);

#if GTK_CHECK_VERSION(3, 0, 0)
static void fm_tab_page_destroy(GtkWidget *page);
#else
static void fm_tab_page_destroy(GtkObject *page);
#endif

static void fm_tab_page_realize(GtkWidget *page);
static void fm_tab_page_unrealize(GtkWidget *page);

static GQuark popup_qdata;

G_DEFINE_TYPE(FmTabPage, fm_tab_page, GTK_TYPE_HPANED)

static void fm_tab_page_class_init(FmTabPageClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
#if GTK_CHECK_VERSION(3, 0, 0)
    widget_class->destroy = fm_tab_page_destroy;
#else
    GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS(klass);
    gtk_object_class->destroy = fm_tab_page_destroy;
#endif
    g_object_class->finalize = fm_tab_page_finalize;
    widget_class->realize = fm_tab_page_realize;
    widget_class->unrealize = fm_tab_page_unrealize;

    /* signals that current working directory is changed. */
    signals[CHDIR] =
        g_signal_new("chdir",
                    G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (FmTabPageClass, chdir),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

#if 0
    /* FIXME: is this really needed? */
    /* signals that the user wants to open a new dir. */
    signals[OPEN_DIR] =
        g_signal_new("open-dir",
                    G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (FmTabPageClass, open_dir),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__UINT_POINTER,
                    G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
#endif

    /* emit when the status bar message is changed */
    signals[STATUS] =
        g_signal_new("status",
                    G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (FmTabPageClass, status),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__UINT_POINTER,
                    G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
    /* folder view received the focus */
    signals[GOT_FOCUS] =
        g_signal_new("got-focus",
                    G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (FmTabPageClass, got_focus),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
    /* the folder finished loading */
    signals[LOADED] =
        g_signal_new("loaded",
                    G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (FmTabPageClass, loaded),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

    popup_qdata = g_quark_from_static_string("tab-page::popup-filelist");
}


static void fm_tab_page_finalize(GObject *object)
{
    FmTabPage *page;
    int i;

    g_return_if_fail(object != NULL);
    g_return_if_fail(FM_IS_TAB_PAGE(object));

    page = (FmTabPage*)object;

    for(i = 0; i < FM_STATUS_TEXT_NUM; ++i)
        g_free(page->status_text[i]);

#if FM_CHECK_VERSION(1, 0, 2)
    g_free(page->filter_pattern);
#endif

    G_OBJECT_CLASS(fm_tab_page_parent_class)->finalize(object);
}

static void free_folder(FmTabPage* page)
{
    if(page->folder)
    {
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_start_loading, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_finish_loading, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_fs_info, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_error, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_content_changed, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_removed, page);
        g_signal_handlers_disconnect_by_func(page->folder, on_folder_unmount, page);
        g_object_unref(page->folder);
        page->folder = NULL;
#if FM_CHECK_VERSION(1, 2, 0)
        if (page->want_focus)
            fm_path_unref(page->want_focus);
        page->want_focus = NULL;
#endif
    }
}

/* workaround on FmStandardView: it should forward focus-in events but doesn't do */
static void on_folder_view_add(GtkContainer *container, GtkWidget *child, FmTabPage *page)
{
    g_signal_connect(child, "focus-in-event",
                     G_CALLBACK(on_folder_view_focus_in), page);
}

static void on_folder_view_remove(GtkContainer *container, GtkWidget *child, FmTabPage *page)
{
    g_signal_handlers_disconnect_by_func(child, on_folder_view_focus_in, page);
}

static void _connect_focus_in(FmFolderView *folder_view, FmTabPage *page)
{
    GList *children, *l;

    g_signal_connect(folder_view, "focus-in-event",
                     G_CALLBACK(on_folder_view_focus_in), page);
    g_signal_connect(folder_view, "add",
                     G_CALLBACK(on_folder_view_add), page);
    g_signal_connect(folder_view, "remove",
                     G_CALLBACK(on_folder_view_remove), page);
    children = gtk_container_get_children(GTK_CONTAINER(folder_view));
    for (l = children; l; l = l->next)
        g_signal_connect(l->data, "focus-in-event",
                         G_CALLBACK(on_folder_view_focus_in), page);
    g_list_free(children);
}

static void _disconnect_focus_in(FmFolderView *folder_view, FmTabPage *page)
{
    GList *children, *l;

    g_signal_handlers_disconnect_by_func(folder_view, on_folder_view_focus_in, page);
    g_signal_handlers_disconnect_by_func(folder_view, on_folder_view_add, page);
    g_signal_handlers_disconnect_by_func(folder_view, on_folder_view_remove, page);
    children = gtk_container_get_children(GTK_CONTAINER(folder_view));
    for (l = children; l; l = l->next)
        g_signal_handlers_disconnect_by_func(l->data, on_folder_view_focus_in, page);
    g_list_free(children);
}

#if FM_CHECK_VERSION(1, 2, 0)
static void on_home_path_changed(FmAppConfig *cfg, FmSidePane *sp)
{
    if (cfg->home_path && cfg->home_path[0])
        fm_side_pane_set_home_dir(sp, cfg->home_path);
    else
        fm_side_pane_set_home_dir(sp, fm_get_home_dir());
}
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
void fm_tab_page_destroy(GtkWidget *object)
#else
void fm_tab_page_destroy(GtkObject *object)
#endif
{
    FmTabPage* page = FM_TAB_PAGE(object);

    g_debug("fm_tab_page_destroy, folder: %s",
            page->folder ? fm_path_get_basename(fm_folder_get_path(page->folder)) : "(none)");
    free_folder(page);
    if(page->nav_history)
    {
        g_object_unref(page->nav_history);
        page->nav_history = NULL;
    }
    if(page->folder_view)
    {
        /* tab page may be inactive now and there is a chance it does not contain
           own view therefore we have to destroy own folder view widget manually */
        GtkWidget *fv = GTK_WIDGET(page->folder_view);
        GtkWidget *parent = gtk_widget_get_parent(fv);
        GList *panes, *l;

        if (parent)
        {
            panes = gtk_container_get_children(GTK_CONTAINER(page->views));
            for (l = panes; l; l = l->next)
                if ((GtkWidget*)l->data == fv)
                    break;
            if (l == NULL)
                gtk_container_remove(GTK_CONTAINER(parent), fv);
            g_list_free(panes);
        }

        g_signal_handlers_disconnect_by_func(page->folder_view, on_folder_view_sel_changed, page);
#if FM_CHECK_VERSION(1, 2, 0)
        g_signal_handlers_disconnect_by_func(page->folder_view, on_folder_view_columns_changed, page);
#endif
#if FM_CHECK_VERSION(1, 2, 0)
        g_signal_handlers_disconnect_by_func(app_config, on_home_path_changed, page->side_pane);
#endif
        _disconnect_focus_in(page->folder_view, page);
        g_object_unref(page->folder_view);
        page->folder_view = NULL;
    }
#if FM_CHECK_VERSION(1, 0, 2)
    g_strfreev(page->columns);
    page->columns = NULL;
#endif
    if(page->update_scroll_id)
    {
        g_source_remove(page->update_scroll_id);
        page->update_scroll_id = 0;
    }
#if FM_CHECK_VERSION(1, 2, 0)
    fm_side_pane_set_popup_updater(page->side_pane, NULL, NULL);
#endif
    if (page->dd)
    {
        g_object_unref(page->dd);
        page->dd = NULL;
    }

#if GTK_CHECK_VERSION(3, 0, 0)
    if(GTK_WIDGET_CLASS(fm_tab_page_parent_class)->destroy)
        (*GTK_WIDGET_CLASS(fm_tab_page_parent_class)->destroy)(object);
#else
    if(GTK_OBJECT_CLASS(fm_tab_page_parent_class)->destroy)
        (*GTK_OBJECT_CLASS(fm_tab_page_parent_class)->destroy)(object);
#endif
}

static void on_folder_content_changed(FmFolder* folder, FmTabPage* page)
{
    /* update status text */
    g_free(page->status_text[FM_STATUS_TEXT_NORMAL]);
    page->status_text[FM_STATUS_TEXT_NORMAL] = format_status_text(page);
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_NORMAL,
                  page->status_text[FM_STATUS_TEXT_NORMAL]);
}

static void on_folder_view_sel_changed(FmFolderView* fv, gint n_sel, FmTabPage* page)
{
    char* msg = page->status_text[FM_STATUS_TEXT_SELECTED_FILES];
    GString *str;
    g_free(msg);

    if(n_sel > 0)
    {
        str = g_string_sized_new(64);
        /* FIXME: display total size of all selected files. */
        if(n_sel == 1) /* only one file is selected */
        {
            FmFileInfoList* files = fm_folder_view_dup_selected_files(fv);
            FmFileInfo* fi = fm_file_info_list_peek_head(files);
            const char* size_str = fm_file_info_get_disp_size(fi);
#if FM_CHECK_VERSION(1, 2, 0)
            GList *l;
#endif
            if(size_str)
            {
                g_string_printf(str, "\"%s\" (%s) %s",
                            fm_file_info_get_disp_name(fi),
                            size_str ? size_str : "",
                            fm_file_info_get_desc(fi));
            }
            else
            {
                g_string_printf(str, "\"%s\" %s",
                            fm_file_info_get_disp_name(fi),
                            fm_file_info_get_desc(fi));
            }
#if FM_CHECK_VERSION(1, 2, 0)
            /* ---- statusbar plugins support ---- */
            CHECK_MODULES();
            for (l = _tab_page_modules; l; l = l->next)
            {
                FmTabPageStatusInit *module = l->data;
                char *message = module->sel_message(files, n_sel);
                if (message && message[0])
                {
                    g_string_append_c(str, ' ');
                    g_string_append(str, message);
                }
                g_free(message);
            }
#endif
            fm_file_info_list_unref(files);
        }
        else
        {
            FmFileInfoList* files;
            goffset sum;
            GList *l;
            char size_str[128];

            g_string_printf(str, ngettext("%d item selected", "%d items selected", n_sel), n_sel);
            /* don't count if too many files are selected, that isn't lightweight */
            if (n_sel < 1000)
            {
                sum = 0;
                files = fm_folder_view_dup_selected_files(fv);
                for (l = fm_file_info_list_peek_head_link(files); l; l = l->next)
                {
                    if (fm_file_info_is_dir(l->data))
                    {
                        /* if we got a directory then we cannot tell it's size
                           unless we do deep count but we cannot afford it */
                        sum = -1;
                        break;
                    }
                    sum += fm_file_info_get_size(l->data);
                }
                if (sum >= 0)
                {
                    fm_file_size_to_str(size_str, sizeof(size_str), sum,
                                        fm_config->si_unit);
                    g_string_append_printf(str, " (%s)", size_str);
                }
#if FM_CHECK_VERSION(1, 2, 0)
                /* ---- statusbar plugins support ---- */
                CHECK_MODULES();
                for (l = _tab_page_modules; l; l = l->next)
                {
                    FmTabPageStatusInit *module = l->data;
                    char *message = module->sel_message(files, n_sel);
                    if (message && message[0])
                    {
                        g_string_append_c(str, ' ');
                        g_string_append(str, message);
                    }
                    g_free(message);
                }
#endif
                fm_file_info_list_unref(files);
            }
            /* FIXME: can we show some more info on selection?
               that isn't lightweight if a lot of files are selected */
        }
        msg = g_string_free(str, FALSE);
    }
    else
        msg = NULL;
    page->status_text[FM_STATUS_TEXT_SELECTED_FILES] = msg;
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_SELECTED_FILES, msg);
}

#if FM_CHECK_VERSION(1, 2, 0)
static void  on_folder_view_columns_changed(FmFolderView *fv, FmTabPage *page)
{
    GSList *columns = fm_folder_view_get_columns(fv), *l;
    char **cols;
    guint i;

    if (columns == NULL)
        return;
    i = g_slist_length(columns);
    cols = g_new(char *, i+1);
    for (i = 0, l = columns; l; i++, l = l->next)
    {
        FmFolderViewColumnInfo *info = l->data;

        if (info->width > 0)
            cols[i] = g_strdup_printf("%s:%d",
                                      fm_folder_model_col_get_name(info->col_id),
                                      info->width);
        else
            cols[i] = g_strdup(fm_folder_model_col_get_name(info->col_id));
    }
    g_slist_free(columns);
    cols[i] = NULL; /* terminate the list */
    if (page->own_config)
    {
        g_strfreev(page->columns);
        page->columns = cols;
        fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv),
                                           page->sort_type, page->sort_by, -1,
                                           page->show_hidden, cols);
    }
    else
    {
        g_strfreev(app_config->columns);
        app_config->columns = cols;
        pcmanfm_save_config(FALSE);
    }
}
#endif

static gboolean on_folder_view_focus_in(GtkWidget *widget, GdkEvent *event, FmTabPage *page)
{
    g_signal_emit(page, signals[GOT_FOCUS], 0);
    return FALSE;
}

static FmJobErrorAction on_folder_error(FmFolder* folder, GError* err, FmJobErrorSeverity severity, FmTabPage* page)
{
    GtkWindow* win = GTK_WINDOW(GET_MAIN_WIN(page));
    if(err->domain == G_IO_ERROR)
    {
        if( err->code == G_IO_ERROR_NOT_MOUNTED && severity < FM_JOB_ERROR_CRITICAL )
        {
            FmPath* path = fm_folder_get_path(folder);
            if(fm_mount_path(win, path, TRUE))
                return FM_JOB_RETRY;
        }
    }
    if(severity >= FM_JOB_ERROR_MODERATE)
    {
        /* Only show more severe errors to the users and
         * ignore milder errors. Otherwise too many error
         * message boxes can be annoying.
         * This fixes bug #3411298- Show "Permission denied" when switching to super user mode.
         * https://sourceforge.net/tracker/?func=detail&aid=3411298&group_id=156956&atid=801864
         * */
        fm_show_error(win, NULL, err->message);
    }
    return FM_JOB_CONTINUE;
}

static void fm_tab_page_realize(GtkWidget *page)
{
    GTK_WIDGET_CLASS(fm_tab_page_parent_class)->realize(page);
    if (FM_TAB_PAGE(page)->busy)
        fm_set_busy_cursor(page);
}

static void fm_tab_page_unrealize(GtkWidget *page)
{
    if (FM_TAB_PAGE(page)->busy)
        fm_unset_busy_cursor(page);
    GTK_WIDGET_CLASS(fm_tab_page_parent_class)->unrealize(page);
}

static void _tab_set_busy_cursor(FmTabPage* page)
{
    page->busy = TRUE;
    if (gtk_widget_get_realized(GTK_WIDGET(page)))
        fm_set_busy_cursor(GTK_WIDGET(page));
}

static void _tab_unset_busy_cursor(FmTabPage* page)
{
    page->busy = FALSE;
    if (gtk_widget_get_realized(GTK_WIDGET(page)))
        fm_unset_busy_cursor(GTK_WIDGET(page));
}

#if FM_CHECK_VERSION(1, 0, 2)
static gboolean fm_tab_page_path_filter(FmFileInfo *file, gpointer user_data)
{
    FmTabPage *page;
    const char *disp_name;
    char *casefold, *key;
    gboolean result;

    g_return_val_if_fail(FM_IS_TAB_PAGE(user_data), FALSE);
    page = (FmTabPage*)user_data;
    if (page->filter_pattern == NULL)
        return TRUE;
    disp_name = fm_file_info_get_disp_name(file);
    casefold = g_utf8_casefold(disp_name, -1);
    key = g_utf8_normalize(casefold, -1, G_NORMALIZE_ALL);
    g_free(casefold);
    result = (fnmatch(page->filter_pattern, key, 0) == 0);
    g_free(key);
    return result;
}
#endif

static void on_folder_start_loading(FmFolder* folder, FmTabPage* page)
{
    FmFolderView* fv = page->folder_view;
    /* g_debug("start-loading"); */
    /* FIXME: this should be set on toplevel parent */
    _tab_set_busy_cursor(page);

#if FM_CHECK_VERSION(1, 0, 2)
    if(fm_folder_is_incremental(folder))
    {
        /* create a model for the folder and set it to the view
           it is delayed for non-incremental folders since adding rows into
           model is much faster without handlers connected to its signals */
        FmFolderModel* model = fm_folder_model_new(folder, page->show_hidden);
        if (page->filter_pattern)
        {
            fm_folder_model_add_filter(model, fm_tab_page_path_filter, page);
            fm_folder_model_apply_filters(model);
        }
        fm_folder_view_set_model(fv, model);
        fm_folder_model_set_sort(model, page->sort_by, page->sort_type);
        g_object_unref(model);
    }
    else
#endif
        fm_folder_view_set_model(fv, NULL);
}

static gboolean update_scroll(gpointer data)
{
    FmTabPage* page = data;
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(page->folder_view);
#if !FM_CHECK_VERSION(1, 0, 2)
    const FmNavHistoryItem* item;

    item = fm_nav_history_get_cur(page->nav_history);
    /* scroll to recorded position */
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(scroll), item->scroll_pos);
#else
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(scroll),
                             fm_nav_history_get_scroll_pos(page->nav_history));
#endif
#if FM_CHECK_VERSION(1, 2, 0)
    if (page->want_focus)
    {
        fm_folder_view_select_file_path(page->folder_view, page->want_focus);
        fm_folder_view_scroll_to_path(page->folder_view, page->want_focus, TRUE);
        fm_path_unref(page->want_focus);
        page->want_focus = NULL;
    }
#endif
    page->update_scroll_id = 0;
    return FALSE;
}

static void on_folder_finish_loading(FmFolder* folder, FmTabPage* page)
{
    FmFolderView* fv = page->folder_view;

    /* Note: most of the time, we delay the creation of the 
     * folder model and do it after the whole folder is loaded.
     * That is because adding rows into model is much faster when no handlers
     * are connected to its signals. So we detach the model from folder view
     * and create the model again when it's fully loaded. 
     * This optimization, however, is not used for FmFolder objects
     * with incremental loading (search://) */
    if(fm_folder_view_get_model(fv) == NULL)
    {
        /* create a model for the folder and set it to the view */
        FmFolderModel* model = fm_folder_model_new(folder, page->show_hidden);
        fm_folder_view_set_model(fv, model);
#if FM_CHECK_VERSION(1, 0, 2)
        if (page->filter_pattern)
        {
            fm_folder_model_add_filter(model, fm_tab_page_path_filter, page);
            fm_folder_model_apply_filters(model);
        }
        /* since 1.0.2 sorting should be applied on model instead of view */
        fm_folder_model_set_sort(model, page->sort_by, page->sort_type);
#endif
        g_object_unref(model);
    }
    fm_folder_query_filesystem_info(folder); /* FIXME: is this needed? */

    // fm_path_entry_set_path(entry, path);
    /* delaying scrolling since drawing folder view is delayed */
    if(!page->update_scroll_id)
        page->update_scroll_id = gdk_threads_add_timeout(50, update_scroll, page);

    /* update status bar */
    /* update status text */
    g_free(page->status_text[FM_STATUS_TEXT_NORMAL]);
    page->status_text[FM_STATUS_TEXT_NORMAL] = format_status_text(page);
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_NORMAL,
                  page->status_text[FM_STATUS_TEXT_NORMAL]);

    _tab_unset_busy_cursor(page);
    /* g_debug("finish-loading"); */
    g_signal_emit(page, signals[LOADED], 0);
}

static void on_folder_unmount(FmFolder* folder, FmTabPage* page)
{
    if (app_config->close_on_unmount)
        gtk_widget_destroy(GTK_WIDGET(page));
    else
#if FM_CHECK_VERSION(1, 2, 0)
    if (app_config->home_path && app_config->home_path[0])
    {
        FmPath *path = fm_path_new_for_str(app_config->home_path);

        fm_tab_page_chdir(page, path);
        fm_path_unref(path);
    }
    else
#endif
        fm_tab_page_chdir(page, fm_path_get_home());
}

static void on_folder_removed(FmFolder* folder, FmTabPage* page)
{
    if (app_config->close_on_unmount)
        gtk_widget_destroy(GTK_WIDGET(page));
    else
#if FM_CHECK_VERSION(1, 2, 0)
    if (app_config->home_path && app_config->home_path[0])
    {
        FmPath *path = fm_path_new_for_str(app_config->home_path);

        fm_tab_page_chdir(page, path);
        fm_path_unref(path);
    }
    else
#endif
        fm_tab_page_chdir(page, fm_path_get_home());
}

static void on_folder_fs_info(FmFolder* folder, FmTabPage* page)
{
    guint64 free, total;
    char* msg = page->status_text[FM_STATUS_TEXT_FS_INFO];
    g_free(msg);
    /* g_debug("%p, fs-info: %d", folder, (int)folder->has_fs_info); */
    if(fm_folder_get_filesystem_info(folder, &total, &free))
    {
        char total_str[ 64 ];
        char free_str[ 64 ];
        fm_file_size_to_str(free_str, sizeof(free_str), free, fm_config->si_unit);
        fm_file_size_to_str(total_str, sizeof(total_str), total, fm_config->si_unit);
        msg = g_strdup_printf(_("Free space: %s (Total: %s)"), free_str, total_str );
    }
    else
        msg = NULL;
    page->status_text[FM_STATUS_TEXT_FS_INFO] = msg;
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_FS_INFO, msg);
}

static char* format_status_text(FmTabPage* page)
{
    FmFolderModel* model = fm_folder_view_get_model(page->folder_view);
    FmFolder* folder = fm_folder_view_get_folder(page->folder_view);
    if(model && folder)
    {
        FmFileInfoList* files = fm_folder_get_files(folder);
        GString* msg = g_string_sized_new(128);
        int total_files = fm_file_info_list_get_length(files);
        int shown_files = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL);
        int hidden_files = total_files - shown_files;
        const char* visible_fmt = ngettext("%d item", "%d items", shown_files);
        const char* hidden_fmt = ngettext(" (%d hidden)", " (%d hidden)", hidden_files);

        g_string_append_printf(msg, visible_fmt, shown_files);
        if(hidden_files > 0)
            g_string_append_printf(msg, hidden_fmt, hidden_files);
        return g_string_free(msg, FALSE);
    }
    return NULL;
}

static void on_open_in_new_tab(GtkAction* act, FmMainWin* win)
{
    GObject* act_grp;
    FmFileInfoList* sels;
    GList* l;

    g_object_get(act, "action-group", &act_grp, NULL);
    sels = g_object_get_qdata(act_grp, popup_qdata);
    g_object_unref(act_grp);
    for( l = fm_file_info_list_peek_head_link(sels); l; l=l->next )
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_add_tab(win, fm_file_info_get_path(fi));
    }
}

static void on_open_in_new_win(GtkAction* act, FmMainWin* win)
{
    GObject* act_grp;
    FmFileInfoList* sels;
    GList* l;

    g_object_get(act, "action-group", &act_grp, NULL);
    sels = g_object_get_qdata(act_grp, popup_qdata);
    g_object_unref(act_grp);
    for( l = fm_file_info_list_peek_head_link(sels); l; l=l->next )
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_add_win(win, fm_file_info_get_path(fi));
    }
}

static void on_open_folder_in_terminal(GtkAction* act, FmMainWin* win)
{
    GObject* act_grp;
    FmFileInfoList* files;
    GList* l;

    g_object_get(act, "action-group", &act_grp, NULL);
    files = g_object_get_qdata(act_grp, popup_qdata);
    g_object_unref(act_grp);
    for(l=fm_file_info_list_peek_head_link(files);l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        pcmanfm_open_folder_in_terminal(GTK_WINDOW(win), fm_file_info_get_path(fi));
    }
}

/* folder view popups */
static void update_files_popup(FmFolderView* fv, GtkWindow* win,
                               GtkUIManager* ui, GtkActionGroup* act_grp,
                               FmFileInfoList* files)
{
    GList* l;
    gboolean all_native = TRUE;

    for(l = fm_file_info_list_peek_head_link(files); l; l = l->next)
        if(!fm_file_info_is_dir(l->data))
            return; /* actions are valid only if all selected are directories */
        else if (!fm_file_info_is_native(l->data))
            all_native = FALSE;
    g_object_set_qdata_full(G_OBJECT(act_grp), popup_qdata,
                            fm_file_info_list_ref(files),
                            (GDestroyNotify)fm_file_info_list_unref);
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, folder_menu_actions,
                                 G_N_ELEMENTS(folder_menu_actions), win);
    gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
    if (!all_native)
        gtk_action_set_visible(gtk_action_group_get_action(act_grp, "Term"), FALSE);
}

static gboolean open_folder_func(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    FmMainWin* win = FM_MAIN_WIN(user_data);
    GList* l = folder_infos;
    FmFileInfo* fi = (FmFileInfo*)l->data;
    fm_main_win_chdir(win, fm_file_info_get_path(fi));
    l=l->next;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_add_tab(win, fm_file_info_get_path(fi));
    }
    return TRUE;
}

#if FM_CHECK_VERSION(1, 2, 0)
void _update_sidepane_popup(FmSidePane* sp, GtkUIManager* ui,
                            GtkActionGroup* act_grp,
                            FmFileInfo* file, gpointer user_data)
{
    FmMainWin *win = GET_MAIN_WIN(user_data); /* user_data is FmTabPage */
    FmFileInfoList* files;

    /* bookmark may contain not a directory */
    if (G_UNLIKELY(!file || !fm_file_info_is_dir(file)))
        return;
    /* well, it should be FmMainWin but let safeguard it */
    if (G_UNLIKELY(!IS_FM_MAIN_WIN(win)))
        return;
    files = fm_file_info_list_new();
    fm_file_info_list_push_tail(files, file);
    g_object_set_qdata_full(G_OBJECT(act_grp), popup_qdata, files,
                            (GDestroyNotify)fm_file_info_list_unref);
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, folder_menu_actions,
                                 G_N_ELEMENTS(folder_menu_actions), win);
    /* we use the same XML for simplicity */
    gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
    if (!fm_file_info_is_native(file))
        gtk_action_set_visible(gtk_action_group_get_action(act_grp, "Term"), FALSE);
}
#endif

static gboolean on_drag_motion(FmTabLabel *label, GdkDragContext *drag_context,
                               gint x, gint y, guint time, FmTabPage *page)
{
    GdkAtom target;
    GdkDragAction action = 0;
    FmFileInfo *file_info = NULL;

    /* if change_tab_on_drop is set then we should ignore it and drop file
       using classic behavior - drop after it unfolded, so not drop on label */
    if (!app_config->change_tab_on_drop && page->folder_view)
        file_info = fm_folder_view_get_cwd_info(page->folder_view);
    fm_dnd_dest_set_dest_file(page->dd, file_info);
    if (file_info == NULL)
        return FALSE; /* not in drop zone */
    target = fm_dnd_dest_find_target(page->dd, drag_context);
    if (target != GDK_NONE && fm_dnd_dest_is_target_supported(page->dd, target))
        action = fm_dnd_dest_get_default_action(page->dd, drag_context, target);
    if (action == 0)
        return FALSE; /* cannot drop on that destination */
    gdk_drag_status(drag_context, action, time);
    return TRUE;
}

static void fm_tab_page_init(FmTabPage *page)
{
    GtkPaned* paned = GTK_PANED(page);
    FmTabLabel* tab_label;
    FmFolderView* folder_view;
    GList* focus_chain = NULL;
    AtkObject *atk_widget, *atk_label;
    AtkRelation *relation;
    FmSidePaneMode mode = app_config->side_pane_mode;

    page->side_pane = fm_side_pane_new();
    fm_side_pane_set_mode(page->side_pane, (mode & FM_SP_MODE_MASK));
#if FM_CHECK_VERSION(1, 2, 0)
    fm_side_pane_set_popup_updater(page->side_pane, _update_sidepane_popup, page);
    if (app_config->home_path && app_config->home_path[0])
        fm_side_pane_set_home_dir(page->side_pane, app_config->home_path);
    g_signal_connect(app_config, "changed::home_path",
                     G_CALLBACK(on_home_path_changed), page->side_pane);
#endif
    /* TODO: add a close button to side pane */
    gtk_paned_add1(paned, GTK_WIDGET(page->side_pane));
    focus_chain = g_list_prepend(focus_chain, page->side_pane);

    /* setup initial view mode for the tab from configuration */
    page->view_mode = app_config->view_mode;

    /* handlers below will be used when FmMainWin detects new page added */
    folder_view = (FmFolderView*)fm_standard_view_new(app_config->view_mode,
                                                      update_files_popup,
                                                      open_folder_func);
    /* FIXME: it is inefficient to set view mode to default one then change
       it per-folder but it will be default in most cases but might it be
       even more inefficient to add an object property for the mode and set
       it in fm_tab_page_init() from the property? let make it later */
    page->folder_view = g_object_ref_sink(folder_view);
    fm_folder_view_set_selection_mode(folder_view, GTK_SELECTION_MULTIPLE);
    page->nav_history = fm_nav_history_new();
    page->views = GTK_BOX(gtk_hbox_new(TRUE, 4));
    gtk_box_pack_start(page->views, GTK_WIDGET(folder_view), TRUE, TRUE, 0);
    gtk_paned_add2(paned, GTK_WIDGET(page->views));
    focus_chain = g_list_prepend(focus_chain, page->views);

    /* We need this to change tab order to focus folder view before left pane. */
    gtk_container_set_focus_chain(GTK_CONTAINER(page), focus_chain);
    g_list_free(focus_chain);

//    gtk_widget_show_all(GTK_WIDGET(page));
//    if(mode & FM_SP_HIDE)
//        gtk_widget_hide(GTK_WIDGET(page->side_pane));

    /* create tab label */
    tab_label = (FmTabLabel*)fm_tab_label_new("");
    gtk_label_set_max_width_chars(tab_label->label, app_config->max_tab_chars);
#if ! GTK_CHECK_VERSION(3, 0, 0)
    gtk_label_set_ellipsize(tab_label->label, PANGO_ELLIPSIZE_END);
#endif
    page->tab_label = tab_label;

    atk_widget = gtk_widget_get_accessible(GTK_WIDGET(folder_view));
    atk_label = gtk_widget_get_accessible(GTK_WIDGET(tab_label));
    relation = atk_relation_new(&atk_widget, 1, ATK_RELATION_LABEL_FOR);
    atk_relation_set_add(atk_object_ref_relation_set(atk_label), relation);
    g_object_unref(relation);

    g_signal_connect(folder_view, "sel-changed",
                     G_CALLBACK(on_folder_view_sel_changed), page);
#if FM_CHECK_VERSION(1, 2, 0)
    g_signal_connect(folder_view, "columns-changed",
                     G_CALLBACK(on_folder_view_columns_changed), page);
#endif
    _connect_focus_in(folder_view, page);
    /*
    g_signal_connect(page->folder_view, "chdir",
                     G_CALLBACK(on_folder_view_chdir), page);
    g_signal_connect(page->folder_view, "loaded",
                     G_CALLBACK(on_folder_view_loaded), page);
    g_signal_connect(page->folder_view, "error",
                     G_CALLBACK(on_folder_view_error), page);
    */

    /* setup D&D on the tab label */
    page->dd = fm_dnd_dest_new_with_handlers(GTK_WIDGET(tab_label));
    g_signal_connect(tab_label, "drag-motion", G_CALLBACK(on_drag_motion), page);

    /* the folder view is already loded, call the "loaded" callback ourself. */
    //if(fm_folder_view_is_loaded(folder_view))
    //    on_folder_view_loaded(folder_view, fm_folder_view_get_cwd(folder_view), page);
    page->busy = FALSE;
}

FmTabPage *fm_tab_page_new(FmPath* path)
{
    FmTabPage* page = (FmTabPage*)g_object_new(FM_TYPE_TAB_PAGE, NULL);

    fm_tab_page_chdir(page, path);
    return page;
}

static void fm_tab_page_chdir_without_history(FmTabPage* page, FmPath* path)
{
    char* disp_name = fm_path_display_basename(path);
    char *disp_path;
    FmStandardViewMode view_mode;
    gboolean show_hidden;
    char **columns; /* unused with libfm < 1.0.2 */
#if FM_CHECK_VERSION(1, 2, 0)
    FmPath *prev_path = NULL;
#endif

#if FM_CHECK_VERSION(1, 0, 2)
    if (page->filter_pattern && page->filter_pattern[0])
    {
        /* include pattern into page title */
        char *text = g_strdup_printf("%s [%s]", disp_name, page->filter_pattern);
        g_free(disp_name);
        disp_name = text;
    }
#endif
    fm_tab_label_set_text(page->tab_label, disp_name);
    g_free(disp_name);

#if FM_CHECK_VERSION(1, 2, 0)
    if (app_config->focus_previous && page->folder)
    {
        prev_path = fm_folder_get_path(page->folder);
        if (fm_path_equal(fm_path_get_parent(prev_path), path))
            fm_path_ref(prev_path);
        else
            prev_path = NULL;
    }
#endif

    disp_path = fm_path_display_name(path, FALSE);
    fm_tab_label_set_tooltip_text(FM_TAB_LABEL(page->tab_label), disp_path);
    g_free(disp_path);

    free_folder(page);

    page->folder = fm_folder_from_path(path);
    g_signal_connect(page->folder, "start-loading", G_CALLBACK(on_folder_start_loading), page);
    g_signal_connect(page->folder, "finish-loading", G_CALLBACK(on_folder_finish_loading), page);
    g_signal_connect(page->folder, "error", G_CALLBACK(on_folder_error), page);
    g_signal_connect(page->folder, "fs-info", G_CALLBACK(on_folder_fs_info), page);
    /* destroy the page when the folder is unmounted or deleted. */
    g_signal_connect(page->folder, "removed", G_CALLBACK(on_folder_removed), page);
    g_signal_connect(page->folder, "unmount", G_CALLBACK(on_folder_unmount), page);
    g_signal_connect(page->folder, "content-changed", G_CALLBACK(on_folder_content_changed), page);

#if FM_CHECK_VERSION(1, 2, 0)
    page->want_focus = prev_path;
#endif

    /* get sort and view modes for new path */
    page->own_config = fm_app_config_get_config_for_path(path, &page->sort_type,
                                                         &page->sort_by,
                                                         &view_mode,
                                                         &show_hidden, &columns);
    if (!page->own_config)
        /* bug #3615242: view mode is reset to default when changing directory */
        view_mode = page->view_mode;
    page->show_hidden = show_hidden;
    /* SF bug #898: settings from next folder are saved on previous if
       show_hidden is different: we have to apply folder to the view first */
    g_signal_handlers_block_matched(page->folder_view, G_SIGNAL_MATCH_DETAIL, 0,
                                    g_quark_try_string("filter-changed"), NULL, NULL, NULL);
    on_folder_start_loading(page->folder, page);
    fm_folder_view_set_show_hidden(page->folder_view, show_hidden);
#if FM_CHECK_VERSION(1, 2, 0)
    fm_side_pane_set_show_hidden(page->side_pane, show_hidden);
#endif
    g_signal_handlers_unblock_matched(page->folder_view, G_SIGNAL_MATCH_DETAIL, 0,
                                      g_quark_try_string("filter-changed"), NULL, NULL, NULL);

    if(fm_folder_is_loaded(page->folder))
    {
        on_folder_finish_loading(page->folder, page);
        on_folder_fs_info(page->folder, page);
    }

    /* change view and sort modes according to new path */
    fm_standard_view_set_mode(FM_STANDARD_VIEW(page->folder_view), view_mode);
#if FM_CHECK_VERSION(1, 0, 2)
    /* update columns from config */
    if (columns)
    {
        guint i, n = g_strv_length(columns);
        FmFolderViewColumnInfo *infos = g_new(FmFolderViewColumnInfo, n);
        GSList *infos_list = NULL;

        for (i = 0; i < n; i++)
        {
            char *name = g_strdup(columns[i]), *delim;

#if FM_CHECK_VERSION(1, 2, 0)
            infos[i].width = 0;
#endif
            delim = strchr(name, ':');
            if (delim)
            {
                *delim++ = '\0';
#if FM_CHECK_VERSION(1, 2, 0)
                infos[i].width = atoi(delim);
#endif
            }
            infos[i].col_id = fm_folder_model_get_col_by_name(name);
            g_free(name);
            infos_list = g_slist_append(infos_list, &infos[i]);
        }
#if FM_CHECK_VERSION(1, 2, 0)
        g_signal_handlers_block_by_func(page->folder_view,
                                        on_folder_view_columns_changed, page);
#endif
        fm_folder_view_set_columns(page->folder_view, infos_list);
#if FM_CHECK_VERSION(1, 2, 0)
        g_signal_handlers_unblock_by_func(page->folder_view,
                                          on_folder_view_columns_changed, page);
#endif
        g_slist_free(infos_list);
        g_free(infos);
        if (page->own_config)
            page->columns = g_strdupv(columns);
    }
#else
    /* since 1.0.2 sorting should be applied on model instead */
    fm_folder_view_sort(page->folder_view, page->sort_type, page->sort_by);
#endif

    fm_side_pane_chdir(page->side_pane, path);

    /* tell the world that our current working directory is changed */
    g_signal_emit(page, signals[CHDIR], 0, path);
}

void fm_tab_page_chdir(FmTabPage* page, FmPath* path)
{
    FmPath* cwd = fm_tab_page_get_cwd(page);
    int scroll_pos;
    if(cwd && path && fm_path_equal(cwd, path))
        return;
    scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view)));
    fm_nav_history_chdir(page->nav_history, path, scroll_pos);
    fm_tab_page_chdir_without_history(page, path);
}

void fm_tab_page_set_show_hidden(FmTabPage* page, gboolean show_hidden)
{
    fm_folder_view_set_show_hidden(page->folder_view, show_hidden);
#if FM_CHECK_VERSION(1, 2, 0)
    fm_side_pane_set_show_hidden(page->side_pane, show_hidden);
#endif
    /* update status text */
    g_free(page->status_text[FM_STATUS_TEXT_NORMAL]);
    page->status_text[FM_STATUS_TEXT_NORMAL] = format_status_text(page);
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_NORMAL,
                  page->status_text[FM_STATUS_TEXT_NORMAL]);
}

FmPath* fm_tab_page_get_cwd(FmTabPage* page)
{
    return page->folder ? fm_folder_get_path(page->folder) : NULL;
}

FmSidePane* fm_tab_page_get_side_pane(FmTabPage* page)
{
    return page->side_pane;
}

FmFolderView* fm_tab_page_get_folder_view(FmTabPage* page)
{
    return page->folder_view;
}

FmFolder* fm_tab_page_get_folder(FmTabPage* page)
{
    return fm_folder_view_get_folder(page->folder_view);
}

FmNavHistory* fm_tab_page_get_history(FmTabPage* page)
{
    return page->nav_history;
}

void fm_tab_page_forward(FmTabPage* page)
{
#if FM_CHECK_VERSION(1, 0, 2)
    guint index = fm_nav_history_get_cur_index(page->nav_history);

    if (index > 0)
#else
    if(fm_nav_history_can_forward(page->nav_history))
#endif
    {
#if !FM_CHECK_VERSION(1, 0, 2)
        const FmNavHistoryItem* item;
#endif
        GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
        int scroll_pos = gtk_adjustment_get_value(vadjustment);
#if FM_CHECK_VERSION(1, 0, 2)
        FmPath *path = fm_nav_history_go_to(page->nav_history, index - 1, scroll_pos);
        fm_tab_page_chdir_without_history(page, path);
#else
        fm_nav_history_forward(page->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(page->nav_history);
        fm_tab_page_chdir_without_history(page, item->path);
#endif
    }
}

void fm_tab_page_back(FmTabPage* page)
{
    if(fm_nav_history_can_back(page->nav_history))
    {
#if !FM_CHECK_VERSION(1, 0, 2)
        const FmNavHistoryItem* item;
#endif
        GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
        int scroll_pos = gtk_adjustment_get_value(vadjustment);
#if FM_CHECK_VERSION(1, 0, 2)
        guint index = fm_nav_history_get_cur_index(page->nav_history);
        FmPath *path = fm_nav_history_go_to(page->nav_history, index + 1, scroll_pos);
        fm_tab_page_chdir_without_history(page, path);
#else
        fm_nav_history_back(page->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(page->nav_history);
        fm_tab_page_chdir_without_history(page, item->path);
#endif
    }
}

#if FM_CHECK_VERSION(1, 0, 2)
void fm_tab_page_history(FmTabPage* page, guint history_item)
{
    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
    int scroll_pos = gtk_adjustment_get_value(vadjustment);
    FmPath *path = fm_nav_history_go_to(page->nav_history, history_item, scroll_pos);
    fm_tab_page_chdir_without_history(page, path);
}
#else
void fm_tab_page_history(FmTabPage* page, GList* history_item_link)
{
    const FmNavHistoryItem* item = (FmNavHistoryItem*)history_item_link->data;
    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
    int scroll_pos = gtk_adjustment_get_value(vadjustment);
    fm_nav_history_jump(page->nav_history, history_item_link, scroll_pos);
    item = fm_nav_history_get_cur(page->nav_history);
    fm_tab_page_chdir_without_history(page, item->path);
}
#endif

const char* fm_tab_page_get_title(FmTabPage* page)
{
    FmTabLabel* label = page->tab_label;
    return gtk_label_get_text(label->label);
}

const char* fm_tab_page_get_status_text(FmTabPage* page, FmStatusTextType type)
{
    return (type < FM_STATUS_TEXT_NUM) ? page->status_text[type] : NULL;
}

void fm_tab_page_reload(FmTabPage* page)
{
    FmFolder* folder = fm_folder_view_get_folder(page->folder_view);

    if(folder)
    {
        GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
        int scroll_pos = gtk_adjustment_get_value(vadjustment);
        /* save the scroll position before reload */
#if FM_CHECK_VERSION(1, 0, 2)
        int idx = fm_nav_history_get_cur_index(page->nav_history);
        fm_nav_history_go_to(page->nav_history, idx, scroll_pos);
#else
        FmNavHistoryItem* item = (FmNavHistoryItem*)fm_nav_history_get_cur(page->nav_history);
        /* NOTE: ignoring const modifier due to invalid pre-1.0.2 design */
        item->scroll_pos = scroll_pos;
#endif
        fm_folder_reload(folder);
    }
}

/**
 * fm_tab_page_take_view_back
 * @page: the page instance
 *
 * If folder view that is bound to page isn't present in the container,
 * then moves it from the container where it is currently, to the @page.
 * This API should be called only in single-pane mode, for two-pane use
 * fm_tab_page_set_passive_view() instead.
 *
 * Returns: %TRUE if folder was not in @page and moved successfully.
 */
gboolean fm_tab_page_take_view_back(FmTabPage *page)
{
    GList *panes, *l;
    GtkWidget *folder_view = GTK_WIDGET(page->folder_view);

    gtk_widget_set_state(folder_view, GTK_STATE_NORMAL);
    panes = gtk_container_get_children(GTK_CONTAINER(page->views));
    for (l = panes; l; l = l->next)
        if ((GtkWidget*)l->data == folder_view)
            break;
    g_list_free(panes);
    if (l)
        return FALSE;
    /* we already keep the reference, no need to do it again */
    gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(folder_view)),
                         folder_view);
    gtk_box_pack_start(page->views, folder_view, TRUE, TRUE, 0);
    return TRUE;
}

/**
 * fm_tab_page_set_passive_view
 * @page: the page instance
 * @view: the folder view to add
 * @on_right: %TRUE if @view should be moved to right pane
 *
 * If folder @view isn't already in designed place then moves it from the
 * container where it is currently, to the @page. If @on_right is %TRUE
 * then attempts to move into right pane. If @on_right is %FALSE then
 * attempts to move into left pane.
 *
 * Also if folder view that is bound to @page isn't presented in the
 * container, then moves it from the container where it is currently, to
 * the @page on the side opposite to @view.
 *
 * See also: fm_tab_page_take_view_back().
 *
 * Returns: %TRUE if @view was moved successfully.
 */
gboolean fm_tab_page_set_passive_view(FmTabPage *page, FmFolderView *view,
                                      gboolean on_right)
{
    GtkWidget *pane;

    g_return_val_if_fail(page != NULL && view != NULL, FALSE);
    if (!fm_tab_page_take_view_back(page))
    {
#if !FM_CHECK_VERSION(1, 2, 0)
        /* workaround on ExoIconView bug - it doesn't follow state change
           so we re-add the folder view into our container to force change */
        GtkWidget *fv = GTK_WIDGET(page->folder_view);

        /* we already keep the reference, no need to do it again */
        gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(fv)), fv);
        gtk_box_pack_start(page->views, fv, TRUE, TRUE, 0);
#endif
    }
    pane = GTK_WIDGET(view);
    gtk_widget_set_state(pane, GTK_STATE_ACTIVE);
    g_object_ref(view);
    /* gtk_widget_reparent() is buggy so we do it manually */
    if (gtk_widget_get_parent(pane))
        gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(pane)), pane);
    gtk_box_pack_start(page->views, pane, TRUE, TRUE, 0);
    g_object_unref(view);
    if (!on_right)
        gtk_box_reorder_child(page->views, pane, 0);
    return TRUE;
}

/**
 * fm_tab_page_get_passive_view
 * @page: the page instance
 *
 * Checks if the @page contains some folder view that was added via call
 * to fm_tab_page_set_passive_view() and was not moved out yet.
 *
 * Returns: (transfer none): the folder view or %NULL.
 */
FmFolderView *fm_tab_page_get_passive_view(FmTabPage *page)
{
    GList *panes, *l;
    FmFolderView *view;

    panes = gtk_container_get_children(GTK_CONTAINER(page->views));
    for (l = panes; l; l = l->next)
        if ((FmFolderView*)l->data != page->folder_view)
            break;
    view = l ? l->data : NULL;
    g_list_free(panes);
    return view;
}

#if FM_CHECK_VERSION(1, 0, 2)
/**
 * fm_tab_page_set_filter_pattern
 * @page: the page instance
 * @pattern: (allow-none): new pattern
 *
 * Changes filter for the folder view in the @page. If @pattern is %NULL
 * then folder contents will be not filtered anymore.
 */
void fm_tab_page_set_filter_pattern(FmTabPage *page, const char *pattern)
{
    FmFolderModel *model = NULL;
    char *disp_name;

    /* validate pattern */
    if (pattern && pattern[0] == '\0')
        pattern = NULL;
    if (page->folder_view != NULL)
        model = fm_folder_view_get_model(page->folder_view);
    if (page->filter_pattern == NULL && pattern == NULL)
        return; /* nothing to change */
    /* if we have model then update filter chain in it */
    if (model)
    {
        if (page->filter_pattern == NULL && pattern)
            fm_folder_model_add_filter(model, fm_tab_page_path_filter, page);
        else if (page->filter_pattern && pattern == NULL)
            fm_folder_model_remove_filter(model, fm_tab_page_path_filter, page);
    }
    /* update page own data */
    g_free(page->filter_pattern);
    if (pattern)
    {
        char *casefold = g_utf8_casefold(pattern, -1);
        page->filter_pattern = g_utf8_normalize(casefold, -1, G_NORMALIZE_ALL);
        g_free(casefold);
    }
    else
        page->filter_pattern = NULL;
    /* apply changes if needed */
    if (model)
        fm_folder_model_apply_filters(model);
    /* update tab page title */
    disp_name = fm_path_display_basename(fm_folder_view_get_cwd(page->folder_view));
    if (page->filter_pattern)
    {
        /* include pattern into page title */
        char *text = g_strdup_printf("%s [%s]", disp_name, page->filter_pattern);
        g_free(disp_name);
        disp_name = text;
    }
    fm_tab_label_set_text(page->tab_label, disp_name);
    g_free(disp_name);
}
#endif
