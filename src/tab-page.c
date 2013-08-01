//      fm-tab-page.c
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
#include "pcmanfm.h"

#include "gseal-gtk-compat.h"

/* Additional entries for FmFileMenu popup */
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
static char* format_status_text(FmTabPage* page);

#if GTK_CHECK_VERSION(3, 0, 0)
static void fm_tab_page_destroy(GtkWidget *page);
#else
static void fm_tab_page_destroy(GtkObject *page);
#endif

static void fm_tab_page_realize(GtkWidget *page);
static void fm_tab_page_unrealize(GtkWidget *page);

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
    }
}

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
        g_signal_handlers_disconnect_by_func(page->folder_view, on_folder_view_sel_changed, page);
        page->folder_view = NULL;
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
    g_free(msg);

    if(n_sel > 0)
    {
        /* FIXME: display total size of all selected files. */
        if(n_sel == 1) /* only one file is selected */
        {
            FmFileInfoList* files = fm_folder_view_dup_selected_files(fv);
            FmFileInfo* fi = fm_file_info_list_peek_head(files);
            const char* size_str = fm_file_info_get_disp_size(fi);
            if(size_str)
            {
                msg = g_strdup_printf("\"%s\" (%s) %s",
                            fm_file_info_get_disp_name(fi),
                            size_str ? size_str : "",
                            fm_file_info_get_desc(fi));
            }
            else
            {
                msg = g_strdup_printf("\"%s\" %s",
                            fm_file_info_get_disp_name(fi),
                            fm_file_info_get_desc(fi));
            }
            fm_file_info_list_unref(files);
        }
        else
        {
            msg = g_strdup_printf(ngettext("%d item selected", "%d items selected", n_sel), n_sel);
        }
    }
    else
        msg = NULL;
    page->status_text[FM_STATUS_TEXT_SELECTED_FILES] = msg;
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_SELECTED_FILES, msg);
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
        FmFolderModel* model = fm_folder_model_new(folder, FALSE);
        fm_folder_view_set_model(fv, model);
        fm_folder_model_set_sort(model, app_config->sort_by,
                                 (app_config->sort_type == GTK_SORT_ASCENDING) ?
                                        FM_SORT_ASCENDING : FM_SORT_DESCENDING);
        g_object_unref(model);
    }
    else
#endif
        fm_folder_view_set_model(fv, NULL);
}

static void on_folder_finish_loading(FmFolder* folder, FmTabPage* page)
{
    FmFolderView* fv = page->folder_view;
    const FmNavHistoryItem* item;
    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(fv);

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
        FmFolderModel* model = fm_folder_model_new(folder, app_config->show_hidden);
        fm_folder_view_set_model(fv, model);
#if FM_CHECK_VERSION(1, 0, 2)
        /* since 1.0.2 sorting should be applied on model instead of view */
        fm_folder_model_set_sort(model, app_config->sort_by,
                                 (app_config->sort_type == GTK_SORT_ASCENDING) ?
                                        FM_SORT_ASCENDING : FM_SORT_DESCENDING);
#endif
        g_object_unref(model);
    }
    fm_folder_query_filesystem_info(folder); /* FIXME: is this needed? */

    // fm_path_entry_set_path(entry, path);
    /* scroll to recorded position */
    item = fm_nav_history_get_cur(page->nav_history);
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(scroll), item->scroll_pos);

    /* update status bar */
    /* update status text */
    g_free(page->status_text[FM_STATUS_TEXT_NORMAL]);
    page->status_text[FM_STATUS_TEXT_NORMAL] = format_status_text(page);
    g_signal_emit(page, signals[STATUS], 0,
                  (guint)FM_STATUS_TEXT_NORMAL,
                  page->status_text[FM_STATUS_TEXT_NORMAL]);

    _tab_unset_busy_cursor(page);
    /* g_debug("finish-loading"); */
}

static void on_folder_unmount(FmFolder* folder, FmTabPage* page)
{
    gtk_widget_destroy(GTK_WIDGET(page));
}

static void on_folder_removed(FmFolder* folder, FmTabPage* page)
{
    gtk_widget_destroy(GTK_WIDGET(page));
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
    FmPathList* sels = fm_folder_view_dup_selected_file_paths(win->folder_view);
    GList* l;
    for( l = fm_path_list_peek_head_link(sels); l; l=l->next )
    {
        FmPath* path = (FmPath*)l->data;
        fm_main_win_add_tab(win, path);
    }
    fm_path_list_unref(sels);
}

static void on_open_in_new_win(GtkAction* act, FmMainWin* win)
{
    FmPathList* sels = fm_folder_view_dup_selected_file_paths(win->folder_view);
    GList* l;
    for( l = fm_path_list_peek_head_link(sels); l; l=l->next )
    {
        FmPath* path = (FmPath*)l->data;
        fm_main_win_add_win(win, path);
    }
    fm_path_list_unref(sels);
}

static void on_open_folder_in_terminal(GtkAction* act, FmMainWin* win)
{
    FmFileInfoList* files = fm_folder_view_dup_selected_files(win->folder_view);
    GList* l;
    for(l=fm_file_info_list_peek_head_link(files);l;l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        if(fm_file_info_is_dir(fi) /*&& !fm_file_info_is_virtual(fi)*/)
            pcmanfm_open_folder_in_terminal(GTK_WINDOW(win), fm_file_info_get_path(fi));
    }
    fm_file_info_list_unref(files);
}

/* folder view popups */
static void update_files_popup(FmFolderView* fv, GtkWindow* win,
                               GtkUIManager* ui, GtkActionGroup* act_grp,
                               FmFileInfoList* files)
{
    GList* l;

    for(l = fm_file_info_list_peek_head_link(files); l; l = l->next)
        if(!fm_file_info_is_dir(l->data))
            return; /* actions are valid only if all selected are directories */
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, folder_menu_actions,
                                 G_N_ELEMENTS(folder_menu_actions), win);
    gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
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

static void fm_tab_page_init(FmTabPage *page)
{
    GtkPaned* paned = GTK_PANED(page);
    FmTabLabel* tab_label;
    FmFolderView* folder_view;
    GList* focus_chain = NULL;
    FmSidePaneMode mode = app_config->side_pane_mode;

    page->side_pane = fm_side_pane_new();
    fm_side_pane_set_mode(page->side_pane, (mode & FM_SP_MODE_MASK));
    /* TODO: add a close button to side pane */
    gtk_paned_add1(paned, GTK_WIDGET(page->side_pane));
    focus_chain = g_list_prepend(focus_chain, page->side_pane);

    /* handlers below will be used when FmMainWin detects new page added */
    folder_view = (FmFolderView*)fm_standard_view_new(app_config->view_mode,
                                                      update_files_popup,
                                                      open_folder_func);
    page->folder_view = folder_view;
#if !FM_CHECK_VERSION(1, 0, 2)
    /* since 1.0.2 sorting should be applied on model instead */
    fm_folder_view_sort(folder_view, app_config->sort_type, app_config->sort_by);
#endif
    fm_folder_view_set_selection_mode(folder_view, GTK_SELECTION_MULTIPLE);
    page->nav_history = fm_nav_history_new();
    gtk_paned_add2(paned, GTK_WIDGET(page->folder_view));
    focus_chain = g_list_prepend(focus_chain, page->folder_view);

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

    g_signal_connect(page->folder_view, "sel-changed",
                     G_CALLBACK(on_folder_view_sel_changed), page);
    /*
    g_signal_connect(page->folder_view, "chdir",
                     G_CALLBACK(on_folder_view_chdir), page);
    g_signal_connect(page->folder_view, "loaded",
                     G_CALLBACK(on_folder_view_loaded), page);
    g_signal_connect(page->folder_view, "error",
                     G_CALLBACK(on_folder_view_error), page);
    */

    /* the folder view is already loded, call the "loaded" callback ourself. */
    //if(fm_folder_view_is_loaded(folder_view))
    //    on_folder_view_loaded(folder_view, fm_folder_view_get_cwd(folder_view), page);
    page->busy = FALSE;
}

FmTabPage *fm_tab_page_new(FmPath* path)
{
    FmTabPage* page = (FmTabPage*)g_object_new(FM_TYPE_TAB_PAGE, NULL);

    fm_folder_view_set_show_hidden(page->folder_view, app_config->show_hidden);
    fm_tab_page_chdir(page, path);
    return page;
}

static void fm_tab_page_chdir_without_history(FmTabPage* page, FmPath* path)
{
    char* disp_name = fm_path_display_basename(path);
    fm_tab_label_set_text(page->tab_label, disp_name);
    g_free(disp_name);

    char * disp_path = fm_path_display_name(path, FALSE);
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

    if(fm_folder_is_loaded(page->folder))
    {
        on_folder_start_loading(page->folder, page);
        on_folder_finish_loading(page->folder, page);
        on_folder_fs_info(page->folder, page);
    }
    else
        on_folder_start_loading(page->folder, page);

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
    if(fm_nav_history_can_forward(page->nav_history))
    {
        const FmNavHistoryItem* item;
        GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
        int scroll_pos = gtk_adjustment_get_value(vadjustment);
        fm_nav_history_forward(page->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(page->nav_history);
        fm_tab_page_chdir_without_history(page, item->path);
    }
}

void fm_tab_page_back(FmTabPage* page)
{
    if(fm_nav_history_can_back(page->nav_history))
    {
        const FmNavHistoryItem* item;
        GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
        int scroll_pos = gtk_adjustment_get_value(vadjustment);
        fm_nav_history_back(page->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(page->nav_history);
        fm_tab_page_chdir_without_history(page, item->path);
    }
}

void fm_tab_page_history(FmTabPage* page, GList* history_item_link)
{
    const FmNavHistoryItem* item = (FmNavHistoryItem*)history_item_link->data;
    GtkAdjustment* vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
    int scroll_pos = gtk_adjustment_get_value(vadjustment);
    fm_nav_history_jump(page->nav_history, history_item_link, scroll_pos);
    item = fm_nav_history_get_cur(page->nav_history);
    fm_tab_page_chdir_without_history(page, item->path);
}

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
        fm_folder_reload(folder);
}
