/*
 *      main-win.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012-2018 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <unistd.h> /* for get euid */
#include <sys/types.h>
#include <ctype.h>

#include "pcmanfm.h"

#include "app-config.h"
#include "main-win.h"
#include "pref.h"
#include "tab-page.h"
#include "connect-server.h"

#include "gseal-gtk-compat.h"

#if GTK_CHECK_VERSION(3, 0, 0)
static void fm_main_win_destroy(GtkWidget *object);
#else
static void fm_main_win_destroy(GtkObject *object);
#endif

static void fm_main_win_finalize(GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

static void update_statusbar(FmMainWin* win);

static gboolean on_focus_in(GtkWidget* w, GdkEventFocus* evt);
static gboolean on_key_press_event(GtkWidget* w, GdkEventKey* evt);
static gboolean on_button_press_event(GtkWidget* w, GdkEventButton* evt);
static gboolean on_scroll_event(GtkWidget* w, GdkEventScroll* evt);
static void on_unrealize(GtkWidget* widget);

static void bounce_action(GtkAction* act, FmMainWin* win);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_new_tab(GtkAction* act, FmMainWin* win);
static void on_close_tab(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);
static void on_open(GtkAction* act, FmMainWin* win);

static void on_link(GtkAction* act, FmMainWin* win);
static void on_copy_to(GtkAction* act, FmMainWin* win);
static void on_move_to(GtkAction* act, FmMainWin* win);
static void on_rename(GtkAction* act, FmMainWin* win);
static void on_trash(GtkAction* act, FmMainWin* win);
static void on_del(GtkAction* act, FmMainWin* win);
static void on_copy_path(GtkAction* act, FmMainWin* win);
static void on_preference(GtkAction* act, FmMainWin* win);

static void on_add_bookmark(GtkAction* act, FmMainWin* win);

static void on_go(GtkAction* act, FmMainWin* win);
static void on_go_back(GtkAction* act, FmMainWin* win);
static void on_go_forward(GtkAction* act, FmMainWin* win);
static void on_go_up(GtkAction* act, FmMainWin* win);
static void on_go_home(GtkAction* act, FmMainWin* win);
static void on_go_desktop(GtkAction* act, FmMainWin* win);
static void on_go_trash(GtkAction* act, FmMainWin* win);
static void on_go_computer(GtkAction* act, FmMainWin* win);
static void on_go_network(GtkAction* act, FmMainWin* win);
static void on_go_apps(GtkAction* act, FmMainWin* win);
static void on_go_connect(GtkAction* act, FmMainWin* win);
static void on_reload(GtkAction* act, FmMainWin* win);
#if FM_CHECK_VERSION(1, 0, 2)
static void on_filter(GtkAction* act, FmMainWin* win);
#endif
static void on_show_hidden(GtkToggleAction* act, FmMainWin* win);
#if FM_CHECK_VERSION(1, 2, 0)
static void on_mingle_dirs(GtkToggleAction* act, FmMainWin* win);
#endif
#if FM_CHECK_VERSION(1, 0, 2)
static void on_sort_ignore_case(GtkToggleAction* act, FmMainWin* win);
#endif
static void on_save_per_folder(GtkToggleAction* act, FmMainWin* win);
static void on_show_side_pane(GtkToggleAction* act, FmMainWin* win);
static void on_dual_pane(GtkToggleAction* act, FmMainWin* win);
static void on_show_toolbar(GtkToggleAction *action, FmMainWin *win);
static void on_toolbar_new_win(GtkToggleAction *act, FmMainWin *win);
static void on_toolbar_new_tab(GtkToggleAction *act, FmMainWin *win);
static void on_toolbar_nav(GtkToggleAction *act, FmMainWin *win);
static void on_toolbar_home(GtkToggleAction *act, FmMainWin *win);
static void on_show_status(GtkToggleAction *action, FmMainWin *win);
static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_side_pane_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);
static void on_key_nav_list(GtkAction* act, FmMainWin* win);
static void on_open_in_terminal(GtkAction* act, FmMainWin* win);
/*static void on_open_as_root(GtkAction* act, FmMainWin* win);*/
#if FM_CHECK_VERSION(1, 0, 2)
static void on_search(GtkAction* act, FmMainWin* win);
#endif
#if FM_CHECK_VERSION(1, 2, 0)
static void on_launch(GtkAction* act, FmMainWin* win);
#endif
static void on_fullscreen(GtkToggleAction* act, FmMainWin* win);

static void on_location(GtkAction* act, FmMainWin* win);

static void on_size_increment(GtkAction *act, FmMainWin *win);
static void on_size_decrement(GtkAction *act, FmMainWin *win);
static void on_size_default(GtkAction *act, FmMainWin *win);

static void on_notebook_switch_page(GtkNotebook* nb, gpointer* page, guint num, FmMainWin* win);
static void on_notebook_page_added(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win);
static void on_notebook_page_removed(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win);

#include "main-win-ui.c" /* ui xml definitions and actions */

static GSList* all_wins = NULL;
static GtkAboutDialog* about_dlg = NULL;
static GtkWidget* key_nav_list_dlg = NULL;

static GQuark main_win_qdata;

static void fm_main_win_class_init(FmMainWinClass *klass)
{
    GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
#if GTK_CHECK_VERSION(3, 0, 0)
    widget_class->destroy = fm_main_win_destroy;
#else
    GtkObjectClass* gtk_object_class = GTK_OBJECT_CLASS(klass);
    gtk_object_class->destroy = fm_main_win_destroy;
#endif
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_main_win_finalize;

    widget_class = (GtkWidgetClass*)klass;
    widget_class->focus_in_event = on_focus_in;
    widget_class->key_press_event = on_key_press_event;
    widget_class->button_press_event = on_button_press_event;
    widget_class->scroll_event = on_scroll_event;
    widget_class->unrealize = on_unrealize;

    fm_main_win_parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);

    main_win_qdata = g_quark_from_static_string("FmMainWin::data");
}

static gboolean idle_focus_view(gpointer user_data)
{
    FmMainWin* win = (FmMainWin*)user_data;
    /* window might be destroyed already */
    if(g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    if(win->folder_view)
        gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
    win->idle_handler = 0;
    return FALSE;
}

static void on_location_activate(GtkEntry* entry, FmMainWin* win)
{
    FmPath* path = fm_path_entry_get_path(FM_PATH_ENTRY(entry));

    /* bug #3615243 seems to be thread-related issue, let ref the path now */
    fm_path_ref(path);
    fm_main_win_chdir(win, path);
    fm_path_unref(path);
    if (app_config->pathbar_mode_buttons)
    {
        /* recover from Ctrl+L mode change */
        gtk_widget_hide(GTK_WIDGET(win->location));
        gtk_widget_hide(gtk_ui_manager_get_widget(win->ui, "/toolbar/Go"));
        gtk_widget_show(GTK_WIDGET(win->path_bar));
    }

    /* FIXME: due to bug #650114 in GTK+, GtkEntry still call a
     * idle function for GtkEntryCompletion even if the completion
     * is set to NULL. This causes crash in pcmanfm since libfm
     * set GtkCompletition to NULL when FmPathEntry loses its
     * focus. Hence the bug is triggered when we set focus to
     * the folder view, which in turns causes FmPathEntry to lose focus.
     *
     * See related bug reports for more info:
     * https://bugzilla.gnome.org/show_bug.cgi?id=650114
     * https://sourceforge.net/tracker/?func=detail&aid=3308324&group_id=156956&atid=801864
     *
     * So here is a quick fix:
     * Set the focus to folder view in our own idle handler with lower
     * priority than GtkEntry's idle function (They use G_PRIORITY_HIGH).
     */
    if(win->idle_handler == 0)
        win->idle_handler = gdk_threads_add_idle_full(G_PRIORITY_LOW, idle_focus_view, win, NULL);
}

static void on_path_bar_chdir(FmPathBar *bar, FmPath *path, FmMainWin *win)
{
    fm_main_win_chdir(win, path);
}

static void on_path_bar_mode(GtkRadioAction *act, GtkRadioAction *cur, FmMainWin *win)
{
    int mode = gtk_radio_action_get_current_value(cur);

    if (app_config->pathbar_mode_buttons != mode)
    {
        app_config->pathbar_mode_buttons = mode;
        pcmanfm_save_config(FALSE);
    }
    gtk_widget_set_visible(GTK_WIDGET(win->location), mode == 0);
    gtk_widget_set_visible(gtk_ui_manager_get_widget(win->ui, "/toolbar/Go"), mode == 0);
    gtk_widget_set_visible(GTK_WIDGET(win->path_bar), mode);
}

static void update_sort_menu(FmMainWin* win)
{
    GtkAction* act;
    FmFolderView* fv = win->folder_view;
    GtkSortType type;
#if FM_CHECK_VERSION(1, 0, 2)
    FmFolderModelCol by;
    FmSortMode mode;

    if (win->in_update)
        return;
    win->in_update = TRUE;
    /* we have to update this any time */
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/SavePerFolder");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), win->current_page->own_config);
    win->in_update = FALSE;
    if(fv == NULL || fm_folder_view_get_model(fv) == NULL)
        /* since 1.0.2 libfm have sorting only in FmFolderModel therefore
           if there is no model then we cannot get last sorting from it */
        return;
    if(!fm_folder_model_get_sort(fm_folder_view_get_model(fv), &by, &mode))
        return;
    type = FM_SORT_IS_ASCENDING(mode) ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING;
    /* we don't handle extended modes in radio actions so do that here */
    if(mode != win->current_page->sort_type)
    {
        win->current_page->sort_type = mode;
        if (win->current_page->own_config)
        {
            fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv),
                                               mode, by, -1,
                                               win->current_page->show_hidden,
                                               NULL);
        }
        else
        {
            app_config->sort_type = mode;
            pcmanfm_save_config(FALSE);
        }
    }
#else
    FmFolderModelViewCol by = fm_folder_view_get_sort_by(fv);

    type = fm_folder_view_get_sort_type(fv);
#endif
    win->in_update = TRUE;
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/Asc");
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), type);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/ByName");
#if FM_CHECK_VERSION(1, 0, 2)
    if(by == FM_FOLDER_MODEL_COL_DEFAULT)
        by = FM_FOLDER_MODEL_COL_NAME;
#endif
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), by);
#if FM_CHECK_VERSION(1, 0, 2)
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/SortIgnoreCase");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act),
                                 (mode & FM_SORT_CASE_SENSITIVE) == 0);
#endif
#if FM_CHECK_VERSION(1, 2, 0)
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/MingleDirs");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act),
                                 (mode & FM_SORT_NO_FOLDER_FIRST) != 0);
#endif
    win->in_update = FALSE;
}

static void update_view_menu(FmMainWin* win)
{
    GtkAction* act;
    FmFolderView* fv = win->folder_view;

    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/ShowHidden");
    if (win->in_update)
        return;
    win->in_update = TRUE;
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), fm_folder_view_get_show_hidden(fv));
    gtk_radio_action_set_current_value(win->first_view_mode,
                                       fm_standard_view_get_mode(FM_STANDARD_VIEW(fv)));
    win->in_update = FALSE;
}

static void update_file_menu(FmMainWin* win, FmPath *path)
{
    GtkAction *act;
    /* FmFolderView *fv = win->folder_view; */
    gboolean can_term = pcmanfm_can_open_path_in_terminal(path);

    act = gtk_ui_manager_get_action(win->ui, "/menubar/ToolMenu/Term");
    gtk_action_set_sensitive(act, path && can_term);
#if FM_CHECK_VERSION(1, 2, 0)
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ToolMenu/Launch");
    gtk_action_set_sensitive(act, path && can_term);
#endif
    act = gtk_ui_manager_get_action(win->ui, "/menubar/GoMenu/Up");
    gtk_action_set_sensitive(act, path && fm_path_get_parent(path));
}

static void on_folder_view_sort_changed(FmFolderView* fv, FmMainWin* win)
{
    if(fv != win->folder_view)
        return;
    update_sort_menu(win);
}

static void on_folder_view_sel_changed(FmFolderView* fv, gint n_sel, FmMainWin* win)
{
    GtkAction* act;
    gboolean has_selected = n_sel > 0;

    if(fv != win->folder_view)
        return;
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Open");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Cut");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Copy");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/ToTrash");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Del");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/CopyPath");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Link");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/CopyTo");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/MoveTo");
    gtk_action_set_sensitive(act, has_selected);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/FileProp");
    gtk_action_set_sensitive(act, has_selected);
    /* special handling for 'Rename' option: we can rename only single file,
       and also because GIO doesn't support changing the .desktop files
       display names, therefore we have to disable it in some cases */
    has_selected = FALSE;
    if (n_sel == 1)
    {
        FmFileInfoList *files = fm_folder_view_dup_selected_files(fv);
        FmFileInfo *fi = fm_file_info_list_peek_head(files);
#if FM_CHECK_VERSION(1, 2, 0)
        if (fm_file_info_can_set_name(fi))
#endif
          if (!fm_file_info_is_shortcut(fi) && !fm_file_info_is_desktop_entry(fi))
            has_selected = TRUE;
        fm_file_info_list_unref(files);
    }
    act = gtk_ui_manager_get_action(win->ui, "/menubar/EditMenu/Rename");
    gtk_action_set_sensitive(act, has_selected);
}

static gboolean on_view_key_press_event(FmFolderView* fv, GdkEventKey* evt, FmMainWin* win)
{
    switch(evt->keyval)
    {
    case GDK_KEY_BackSpace:
        on_go_up(NULL, win);
        break;
    }
    return FALSE;
}

static void on_bookmark(GtkMenuItem* mi, FmMainWin* win)
{
    FmPath* path = (FmPath*)g_object_get_qdata(G_OBJECT(mi), main_win_qdata);
    switch(app_config->bm_open_method)
    {
    case FM_OPEN_IN_CURRENT_TAB: /* current tab */
        fm_main_win_chdir(win, path);
        break;
    case FM_OPEN_IN_NEW_TAB: /* new tab */
        fm_main_win_add_tab(win, path);
        break;
    case FM_OPEN_IN_NEW_WINDOW: /* new window */
        fm_main_win_add_win(win, path);
        break;
    }
}

static void create_bookmarks_menu(FmMainWin* win)
{
    const GList *list, *l;
    GtkWidget* mi;
    int i = 0;

#if FM_CHECK_VERSION(1, 0, 2)
    list = fm_bookmarks_get_all(win->bookmarks);
#else
    list = fm_bookmarks_list_all(win->bookmarks);
#endif
    for(l=list;l;l=l->next)
    {
        FmBookmarkItem* item = (FmBookmarkItem*)l->data;
        mi = gtk_image_menu_item_new_with_label(item->name);
        gtk_widget_show(mi);
        g_object_set_qdata_full(G_OBJECT(mi), main_win_qdata,
                                fm_path_ref(item->path), (GDestroyNotify)fm_path_unref);
        g_signal_connect(mi, "activate", G_CALLBACK(on_bookmark), win);
        gtk_menu_shell_insert(win->bookmarks_menu, mi, i);
        ++i;
    }
#if FM_CHECK_VERSION(1, 0, 2)
    g_list_free_full((GList*)list, (GDestroyNotify)fm_bookmark_item_unref);
#endif
    if(i > 0)
    {
        mi = gtk_separator_menu_item_new();
        gtk_widget_show(mi);
        gtk_menu_shell_insert(win->bookmarks_menu, mi, i);
    }
}

static void on_bookmarks_changed(FmBookmarks* bm, FmMainWin* win)
{
    /* delete old items first. */
    GList* mis = gtk_container_get_children(GTK_CONTAINER(win->bookmarks_menu));
    GList* l;
    for(l = mis;l;l=l->next)
    {
        GtkWidget* item = (GtkWidget*)l->data;
        if( g_object_get_qdata(G_OBJECT(item), main_win_qdata) )
        {
            g_signal_handlers_disconnect_by_func(item, on_bookmark, win);
            gtk_widget_destroy(item);
        }
        else
        {
            if(GTK_IS_SEPARATOR_MENU_ITEM(item))
                gtk_widget_destroy(item);
            break;
        }
    }
    g_list_free(mis);

    create_bookmarks_menu(win);
}

static void load_bookmarks(FmMainWin* win, GtkUIManager* ui)
{
    GtkWidget* mi = gtk_ui_manager_get_widget(ui, "/menubar/BookmarksMenu");
    win->bookmarks_menu = GTK_MENU_SHELL(gtk_menu_item_get_submenu(GTK_MENU_ITEM(mi)));
    win->bookmarks = fm_bookmarks_dup();
    g_signal_connect(win->bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), win);

    create_bookmarks_menu(win);
}

static void _update_hist_buttons(FmMainWin* win);

static void on_history_item(GtkMenuItem* mi, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
#if FM_CHECK_VERSION(1, 0, 2)
    guint l = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(mi), main_win_qdata));
#else
    GList* l = g_object_get_qdata(G_OBJECT(mi), main_win_qdata);
#endif
    fm_tab_page_history(page, l);
    _update_hist_buttons(win);
}

static void disconnect_history_item(GtkWidget* mi, gpointer win)
{
    g_signal_handlers_disconnect_by_func(mi, on_history_item, win);
    gtk_widget_destroy(mi);
}

static void on_history_selection_done(GtkMenuShell* menu, gpointer win)
{
    g_debug("history selection done");
    g_signal_handlers_disconnect_by_func(menu, on_history_selection_done, NULL);
    /* cleanup: disconnect and delete all items */
    gtk_container_foreach(GTK_CONTAINER(menu), disconnect_history_item, win);
}

#if FM_CHECK_VERSION(1, 2, 0)
static void on_show_history_menu(FmMenuToolItem* btn, FmMainWin* win)
#else
static void on_show_history_menu(GtkMenuToolButton* btn, FmMainWin* win)
#endif
{
#if FM_CHECK_VERSION(1, 2, 0)
    GtkMenuShell* menu = (GtkMenuShell*)fm_menu_tool_item_get_menu(btn);
#else
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
#endif
#if FM_CHECK_VERSION(1, 0, 2)
    guint i, cur = fm_nav_history_get_cur_index(win->nav_history);
    FmPath* path;
#else
    const GList* l;
    const GList* cur = fm_nav_history_get_cur_link(win->nav_history);
#endif

    /* delete old items */
    gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback)gtk_widget_destroy, NULL);

#if FM_CHECK_VERSION(1, 0, 2)
    for (i = 0; (path = fm_nav_history_get_nth_path(win->nav_history, i)); i++)
    {
#else
    for(l = fm_nav_history_list(win->nav_history); l; l=l->next)
    {
        const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
        FmPath* path = item->path;
#endif
        char* str = fm_path_display_name(path, TRUE);
        GtkWidget* mi;
#if FM_CHECK_VERSION(1, 0, 2)
        if (i == cur)
#else
        if( l == cur )
#endif
        {
            mi = gtk_check_menu_item_new_with_label(str);
            gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(mi), TRUE);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
        }
        else
            mi = gtk_menu_item_new_with_label(str);
        g_free(str);

#if FM_CHECK_VERSION(1, 0, 2)
        g_object_set_qdata(G_OBJECT(mi), main_win_qdata, GUINT_TO_POINTER(i));
#else
        g_object_set_qdata_full(G_OBJECT(mi), main_win_qdata, (gpointer)l, NULL);
#endif
        g_signal_connect(mi, "activate", G_CALLBACK(on_history_item), win);
        gtk_menu_shell_append(menu, mi);
    }
    g_signal_connect(menu, "selection-done", G_CALLBACK(on_history_selection_done), NULL);
    gtk_widget_show_all( GTK_WIDGET(menu) );
}

static void on_tab_page_splitter_pos_changed(GtkPaned* paned, GParamSpec* ps, FmMainWin* win)
{
    GList *children, *child;
    if(paned != (GtkPaned*)win->current_page)
        return;

    app_config->splitter_pos = gtk_paned_get_position(paned);
    pcmanfm_save_config(FALSE);

    /* apply the pos to all other pages as well */
    /* TODO: maybe we should allow different splitter pos for different pages later? */
    children = gtk_container_get_children(GTK_CONTAINER(win->notebook));
    for(child = children; child; child = child->next)
    {
        FmTabPage* page = FM_TAB_PAGE(child->data);
        if((GtkPaned*)page != paned)
            gtk_paned_set_position(GTK_PANED(page), app_config->splitter_pos);
    }
    g_list_free(children);
}

/* This callback is only connected to side pane of current active tab page. */
static void on_side_pane_chdir(FmSidePane* sp, guint button, FmPath* path, FmMainWin* win)
{
    if(sp != win->side_pane)
        return;

    if(button == 2) /* middle click */
    {
        FmPath *prev_path = NULL;

        if (win->folder_view)
            prev_path = fm_folder_view_get_cwd(win->folder_view);
        fm_main_win_add_tab(win, path);
        /* the side pane activates row itself so let reset it back */
        if (prev_path)
            fm_side_pane_chdir(sp, prev_path);
    }
    else
        fm_main_win_chdir(win, path);

    /* bug #3531696: search is done on side pane instead of folder view */
    if(win->folder_view)
        gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
}

/* This callback is only connected to side pane of current active tab page. */
static void on_side_pane_mode_changed(FmSidePane* sp, FmMainWin* win)
{
    FmSidePaneMode mode;

    if(sp != win->side_pane)
        return;

    mode = fm_side_pane_get_mode(sp);

    /* update menu */
    gtk_radio_action_set_current_value(win->first_side_pane_mode, mode);

    if(mode != (app_config->side_pane_mode & FM_SP_MODE_MASK))
    {
        app_config->side_pane_mode = mode | (app_config->side_pane_mode & ~FM_SP_MODE_MASK);
        fm_config_emit_changed(FM_CONFIG(app_config), "side_pane_mode");
        pcmanfm_save_config(FALSE);
    }
}

static void on_always_show_tabs_changed(FmAppConfig *cfg, FmMainWin *win)
{
    /* it will affect only the case when window has exactly 1 tab,
       all other cases will be handled when tab is added or removed */
    if (gtk_notebook_get_n_pages(win->notebook) == 1)
        gtk_notebook_set_show_tabs(win->notebook, app_config->always_show_tabs);
}

static void on_toolsbar_changed(FmAppConfig *cfg, FmMainWin *win)
{
    GtkAction* act;
    GtkWidget *toolitem;
    int n;
    gboolean active;

    if (win->in_update)
        return;
    win->in_update = TRUE; /* avoid recursion */
    active = cfg->tb.visible;
    gtk_widget_set_visible(GTK_WIDGET(win->toolbar), active);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNewWin");
    gtk_action_set_sensitive(act, active);
    if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(act)) != app_config->tb.new_win)
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.new_win);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNewTab");
    gtk_action_set_sensitive(act, active);
    if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(act)) != app_config->tb.new_tab)
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.new_tab);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNav");
    gtk_action_set_sensitive(act, active);
    if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(act)) != app_config->tb.nav)
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.nav);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarHome");
    gtk_action_set_sensitive(act, active);
    if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(act)) != app_config->tb.home)
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.home);
    if (active) /* it it's hidden no reason to update its children */
    {
        toolitem = gtk_ui_manager_get_widget(win->ui, "/toolbar/New");
        gtk_widget_set_visible(toolitem, cfg->tb.new_win);
        toolitem = gtk_ui_manager_get_widget(win->ui, "/toolbar/NewTab");
        gtk_widget_set_visible(toolitem, cfg->tb.new_tab);
        active = cfg->tb.nav;
        toolitem = gtk_ui_manager_get_widget(win->ui, "/toolbar/Next");
        gtk_widget_set_visible(toolitem, active);
        n = gtk_toolbar_get_item_index(win->toolbar, GTK_TOOL_ITEM(toolitem));
        toolitem = GTK_WIDGET(gtk_toolbar_get_nth_item(win->toolbar, n-1));
        gtk_widget_set_visible(toolitem, active); /* Hist */
#if FM_CHECK_VERSION(1, 2, 0)
        toolitem = GTK_WIDGET(gtk_toolbar_get_nth_item(win->toolbar, n-2));
        gtk_widget_set_visible(toolitem, active); /* Prev */
#endif
        toolitem = GTK_WIDGET(gtk_toolbar_get_nth_item(win->toolbar, n+1));
        gtk_widget_set_visible(toolitem, active); /* Up */
        toolitem = gtk_ui_manager_get_widget(win->ui, "/toolbar/Home");
        gtk_widget_set_visible(toolitem, cfg->tb.home);
    }
    win->in_update = FALSE;
}

static void on_statusbar_changed(FmAppConfig *cfg, FmMainWin *win)
{
    gtk_widget_set_visible(GTK_WIDGET(win->statusbar), cfg->show_statusbar);
    update_statusbar(win);
}

static gboolean settings_hack_done = FALSE;
static gint old_gtk_timeout_expand = 0;

static void on_change_tab_on_drop_changed(FmAppConfig *cfg, gpointer _unused)
{
    if (app_config->change_tab_on_drop)
        g_object_set(gtk_settings_get_default(), "gtk-timeout-expand",
                     old_gtk_timeout_expand, NULL);
    else
        g_object_set(gtk_settings_get_default(), "gtk-timeout-expand", 600000, NULL);
}

static void _do_settings_hack(void)
{
    GtkSettings *settings;

    if (settings_hack_done)
        return;
    settings_hack_done = TRUE;
    settings = gtk_settings_get_default();
    g_object_get(settings, "gtk-timeout-expand", &old_gtk_timeout_expand, NULL);
    if (!app_config->change_tab_on_drop)
        /* dirty hack to override crazy GtkNotebook which unfolds tab
           each time it sees some drag comes to the tab label */
        g_object_set(settings, "gtk-timeout-expand", 600000, NULL);
    g_signal_connect(app_config, "changed::change_tab_on_drop",
                     G_CALLBACK(on_change_tab_on_drop_changed), NULL);
}

static void fm_main_win_init(FmMainWin *win)
{
    GtkBox *vbox;
    GtkWidget *menubar;
    GtkBox *pathbox;
    GtkToolItem *toolitem;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAction* act;
    GtkAccelGroup* accel_grp;
    AtkObject *atk_obj, *atk_view;
    AtkRelation *relation;
#if FM_CHECK_VERSION(1, 2, 0)
    GtkRadioAction *mode_action;
    GSList *radio_group;
    GString *str, *xml;
    static char accel_str[] = "<Ctrl>1";
    int i;
    gboolean is_first;
#endif
    GtkShadowType shadow_type;

    pcmanfm_ref();
    all_wins = g_slist_prepend(all_wins, win);

    /* every window should have its own window group.
     * So model dialogs opened for the window does not lockup
     * other windows.
     * This is related to bug #3439056 - Pcman is frozen renaming files. */
    win->win_group = gtk_window_group_new();
    gtk_window_group_add_window(win->win_group, GTK_WINDOW(win));

    gtk_window_set_icon_name(GTK_WINDOW(win), "folder");

    vbox = (GtkBox*)gtk_vbox_new(FALSE, 0);

    /* create menu bar and toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Main");
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), win);
    gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions,
                                        G_N_ELEMENTS(main_win_toggle_actions), win);
#if FM_CHECK_VERSION(1, 2, 0)
    /* generate list of modes dynamically from FmStandardView widget data */
    radio_group = NULL;
    is_first = TRUE;
    str = g_string_new("ViewMode:");
    xml = g_string_new("<menubar><menu action='ViewMenu'><menu action='FolderView'><placeholder name='ViewModes'>");
    accel_str[6] = '1';
    for(i = 0; i < fm_standard_view_get_n_modes(); i++)
    {
        if(fm_standard_view_get_mode_label(i))
        {
            g_string_append(str, fm_standard_view_mode_to_str(i));
            mode_action = gtk_radio_action_new(str->str,
                                               fm_standard_view_get_mode_label(i),
                                               fm_standard_view_get_mode_tooltip(i),
                                               fm_standard_view_get_mode_icon(i),
                                               i);
            gtk_radio_action_set_group(mode_action, radio_group);
            radio_group = gtk_radio_action_get_group(mode_action);
            gtk_action_group_add_action_with_accel(act_grp,
                                                   GTK_ACTION(mode_action),
                                                   accel_str);
            if (is_first) /* work on first one only */
            {
                win->first_view_mode = mode_action;
                g_signal_connect(mode_action, "changed", G_CALLBACK(on_change_mode), win);
            }
            is_first = FALSE;
            g_object_unref(mode_action);
            g_string_append_printf(xml, "<menuitem action='%s'/>", str->str);
            accel_str[6]++; /* <Ctrl>2 and so on */
            g_string_truncate(str, 9); /* reset it to just "ViewMode:" */
        }
    }
    g_string_append(xml, "</placeholder></menu>"); /* it will be continued below */
#else
    gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions,
                                       G_N_ELEMENTS(main_win_mode_actions),
                                       app_config->view_mode,
                                       G_CALLBACK(on_change_mode), win);
#endif
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions,
                                       G_N_ELEMENTS(main_win_sort_type_actions),
#if FM_CHECK_VERSION(1, 0, 2)
                                       FM_SORT_IS_ASCENDING(app_config->sort_type) ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING,
#else
                                       app_config->sort_type,
#endif
                                       G_CALLBACK(on_sort_type), win);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions,
                                       G_N_ELEMENTS(main_win_sort_by_actions),
                                       app_config->sort_by,
                                       G_CALLBACK(on_sort_by), win);
#if FM_CHECK_VERSION(1, 2, 0)
    /* generate list of modes dynamically from FmSidePane widget data */
    radio_group = NULL;
    is_first = TRUE;
    g_string_assign(str, "SidePaneMode:");
    g_string_append(xml, "<menu action='SidePane'><placeholder name='SidePaneModes'>");
    accel_str[6] = '6';
    for(i = 1; i <= fm_side_pane_get_n_modes(); i++)
    {
        if(fm_side_pane_get_mode_label(i))
        {
            g_string_append(str, fm_side_pane_get_mode_name(i));
            mode_action = gtk_radio_action_new(str->str,
                                               fm_side_pane_get_mode_label(i),
                                               fm_side_pane_get_mode_tooltip(i),
                                               NULL,
                                               i);
            gtk_radio_action_set_group(mode_action, radio_group);
            radio_group = gtk_radio_action_get_group(mode_action);
            gtk_action_group_add_action_with_accel(act_grp,
                                                   GTK_ACTION(mode_action),
                                                   accel_str);
            if (is_first) /* work on first one only */
            {
                win->first_side_pane_mode = mode_action;
                g_signal_connect(mode_action, "changed", G_CALLBACK(on_side_pane_mode), win);
            }
            is_first = FALSE;
            g_object_unref(mode_action);
            g_string_append_printf(xml, "<menuitem action='%s'/>", str->str);
            accel_str[6]++; /* <Ctrl>7 and so on */
            g_string_truncate(str, 13); /* reset it to just "SidePaneMode:" */
        }
    }
    gtk_radio_action_set_current_value(win->first_side_pane_mode,
                                       (app_config->side_pane_mode & FM_SP_MODE_MASK));
    g_string_append(xml, "</placeholder></menu></menu></menubar>");
    g_string_free(str, TRUE);
#else
    gtk_action_group_add_radio_actions(act_grp, main_win_side_bar_mode_actions,
                                       G_N_ELEMENTS(main_win_side_bar_mode_actions),
                                       (app_config->side_pane_mode & FM_SP_MODE_MASK),
                                       G_CALLBACK(on_side_pane_mode), win);
#endif
    gtk_action_group_add_radio_actions(act_grp, main_win_path_bar_mode_actions,
                                       G_N_ELEMENTS(main_win_path_bar_mode_actions),
                                       0, G_CALLBACK(on_path_bar_mode), win);

    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(GTK_WINDOW(win), accel_grp);

    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);
#if FM_CHECK_VERSION(1, 2, 0)
    /* add ui generated above */
    gtk_ui_manager_add_ui_from_string(ui, xml->str, xml->len, NULL);
    g_string_free(xml, TRUE);
#else
    act = gtk_ui_manager_get_action(ui, "/menubar/ViewMenu/FolderView/IconView");
    win->first_view_mode = GTK_RADIO_ACTION(act);
    act = gtk_ui_manager_get_action(ui, "/menubar/ViewMenu/SidePane/Places");
    win->first_side_pane_mode = GTK_RADIO_ACTION(act);
#endif
#if !FM_CHECK_VERSION(1, 0, 2)
    act = gtk_ui_manager_get_action(ui, "/menubar/ViewMenu/ShowHidden");
    /* we cannot keep it in sync without callback from folder view which
       is available only in 1.0.2 so just hide it */
    gtk_action_set_visible(act, FALSE);
#endif
    act = gtk_ui_manager_get_action(ui, "/menubar/ViewMenu/SidePane/ShowSidePane");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act),
                                 (app_config->side_pane_mode & FM_SP_HIDE) == 0);

#if FM_CHECK_VERSION(1, 2, 0)
    /* disable "Find Files" button if module isn't available */
    if (!fm_module_is_in_use("vfs", "search"))
    {
        act = gtk_ui_manager_get_action(ui, "/menubar/ToolMenu/Search");
        gtk_action_set_sensitive(act, FALSE);
    }
    /* disable "Applications" button if module isn't available */
    if (!fm_module_is_in_use("vfs", "menu"))
    {
        act = gtk_ui_manager_get_action(ui, "/menubar/GoMenu/Apps");
        gtk_action_set_sensitive(act, FALSE);
    }
#endif

    menubar = gtk_ui_manager_get_widget(ui, "/menubar");
    win->toolbar = GTK_TOOLBAR(gtk_ui_manager_get_widget(ui, "/toolbar"));
    /* FIXME: should make these optional */
    gtk_toolbar_set_icon_size(win->toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(win->toolbar, GTK_TOOLBAR_ICONS);

#if FM_CHECK_VERSION(1, 2, 0)
    /* create history button after 'Prev' and add a popup menu to it */
    toolitem = fm_menu_tool_item_new();
    gtk_toolbar_insert(win->toolbar, toolitem, 3);
#else
    /* create 'Prev' button manually and add a popup menu to it */
    toolitem = (GtkToolItem*)g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);
    gtk_toolbar_insert(win->toolbar, toolitem, 1);
    act = gtk_ui_manager_get_action(ui, "/menubar/GoMenu/Prev");
    gtk_activatable_set_related_action(GTK_ACTIVATABLE(toolitem), act);
#endif

    /* set up history menu */
    win->history_menu = gtk_menu_new();
#if FM_CHECK_VERSION(1, 2, 0)
    fm_menu_tool_item_set_menu(FM_MENU_TOOL_ITEM(toolitem), win->history_menu);
    gtk_tool_item_set_tooltip_text(toolitem, _("Show history of visited folders"));
#else
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolitem), win->history_menu);
    gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(toolitem),
                                                _("Show history of visited folders"));
#endif
    g_signal_connect(toolitem, "show-menu", G_CALLBACK(on_show_history_menu), win);
    atk_obj = gtk_widget_get_accessible(GTK_WIDGET(toolitem));
    if (atk_obj)
        atk_object_set_name(atk_obj, _("History"));

    gtk_box_pack_start( vbox, menubar, FALSE, TRUE, 0 );
    gtk_box_pack_start( vbox, GTK_WIDGET(win->toolbar), FALSE, TRUE, 0 );

    /* load bookmarks menu */
    load_bookmarks(win, ui);

    /* the location bar */
    win->location = fm_path_entry_new();
    g_signal_connect(win->location, "activate", G_CALLBACK(on_location_activate), win);
    if(geteuid() == 0) /* if we're using root, Give the user some warnings */
    {
        GtkWidget* warning = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_widget_set_tooltip_markup(warning, _("You are in super user mode"));

        toolitem = gtk_tool_item_new();
        gtk_container_add( GTK_CONTAINER(toolitem), warning );
        gtk_toolbar_insert(win->toolbar, gtk_separator_tool_item_new(), 0);

        gtk_toolbar_insert(win->toolbar, toolitem, 0);
    }

    win->path_bar = fm_path_bar_new();
    g_signal_connect(win->path_bar, "chdir", G_CALLBACK(on_path_bar_chdir), win);
    pathbox = (GtkBox*)gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(pathbox, GTK_WIDGET(win->location), TRUE, TRUE, 0);
    gtk_box_pack_start(pathbox, GTK_WIDGET(win->path_bar), TRUE, TRUE, 0);

    toolitem = (GtkToolItem*)gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(toolitem), GTK_WIDGET(pathbox));
    gtk_tool_item_set_expand(toolitem, TRUE);
    gtk_toolbar_insert(win->toolbar, toolitem, gtk_toolbar_get_n_items(win->toolbar) - 1);
    g_signal_connect(app_config, "changed::toolsbar",
                     G_CALLBACK(on_toolsbar_changed), win);

    /* notebook - it contains both side pane and folder view(s) */
    win->notebook = (GtkNotebook*)gtk_notebook_new();
    gtk_notebook_set_scrollable(win->notebook, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(win->notebook), 0);
    gtk_notebook_set_show_border(win->notebook, FALSE);
    _do_settings_hack(); /* do it after GtkNotebook initialized */

    /* We need to use connect_after here.
     * GtkNotebook handles the real page switching stuff in default
     * handler of 'switch-page' signal. The current page is changed to the new one
     * after the signal is emitted. So before the default handler is finished,
     * current page is still the old one. */
    g_signal_connect_after(win->notebook, "switch-page", G_CALLBACK(on_notebook_switch_page), win);
    g_signal_connect(win->notebook, "page-added", G_CALLBACK(on_notebook_page_added), win);
    g_signal_connect(win->notebook, "page-removed", G_CALLBACK(on_notebook_page_removed), win);

    gtk_box_pack_start(vbox, GTK_WIDGET(win->notebook), TRUE, TRUE, 0);
    g_signal_connect(app_config, "changed::always_show_tabs",
                     G_CALLBACK(on_always_show_tabs_changed), win);

    /* status bar */
    win->statusbar = (GtkStatusbar*)gtk_statusbar_new();
    /* status bar column showing volume free space */
    gtk_widget_style_get(GTK_WIDGET(win->statusbar), "shadow-type", &shadow_type, NULL);
    win->vol_status = (GtkFrame*)gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(win->vol_status, shadow_type);
    gtk_box_pack_start(GTK_BOX(win->statusbar), GTK_WIDGET(win->vol_status), FALSE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(win->vol_status), gtk_label_new(NULL));

    gtk_box_pack_start( vbox, GTK_WIDGET(win->statusbar), FALSE, TRUE, 0 );
    win->statusbar_ctx = gtk_statusbar_get_context_id(win->statusbar, "status");
    win->statusbar_ctx2 = gtk_statusbar_get_context_id(win->statusbar, "status2");
    g_signal_connect(app_config, "changed::statusbar",
                     G_CALLBACK(on_statusbar_changed), win);

    g_object_unref(act_grp);
    win->ui = ui;

    /* accessibility: setup relation for Go button and location entry */
    atk_obj = gtk_widget_get_accessible(GTK_WIDGET(win->location));
    relation = atk_relation_new(&atk_obj, 1, ATK_RELATION_LABEL_FOR);
    /* use atk_view for button temporarily */
    atk_view = gtk_widget_get_accessible(gtk_ui_manager_get_widget(ui, "/toolbar/Go"));
    atk_relation_set_add(atk_object_ref_relation_set(atk_view), relation);
    g_object_unref(relation);
    /* setup relations with view */
    atk_view = gtk_widget_get_accessible(GTK_WIDGET(win->notebook));
    relation = atk_relation_new(&atk_obj, 1, ATK_RELATION_CONTROLLED_BY);
    atk_relation_set_add(atk_object_ref_relation_set(atk_view), relation);
    g_object_unref(relation);
    atk_obj = gtk_widget_get_accessible(GTK_WIDGET(win->statusbar));
    relation = atk_relation_new(&atk_obj, 1, ATK_RELATION_DESCRIBED_BY);
    atk_relation_set_add(atk_object_ref_relation_set(atk_view), relation);
    g_object_unref(relation);

    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
}


FmMainWin* fm_main_win_new(void)
{
    FmMainWin* win = (FmMainWin*)g_object_new(FM_MAIN_WIN_TYPE, NULL);
    return win;
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void fm_main_win_destroy(GtkWidget *object)
#else
static void fm_main_win_destroy(GtkObject *object)
#endif
{
    FmMainWin *win;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_MAIN_WIN(object));
    win = (FmMainWin*)object;

    /* Gtk+ runs destroy method twice */
    if(win->win_group)
    {
        g_signal_handlers_disconnect_by_func(win->location, on_location_activate, win);
        g_signal_handlers_disconnect_by_func(win->notebook, on_notebook_switch_page, win);
        g_signal_handlers_disconnect_by_func(win->notebook, on_notebook_page_added, win);
        g_signal_handlers_disconnect_by_func(win->notebook, on_notebook_page_removed, win);
        g_signal_handlers_disconnect_by_func(app_config, on_toolsbar_changed, win);
        g_signal_handlers_disconnect_by_func(app_config, on_statusbar_changed, win);
        g_signal_handlers_disconnect_by_func(app_config, on_always_show_tabs_changed, win);

        gtk_window_group_remove_window(win->win_group, GTK_WINDOW(win));
        g_object_unref(win->win_group);
        win->win_group = NULL;
        g_object_unref(win->ui);
        win->ui = NULL;
        if(win->bookmarks)
        {
            g_signal_handlers_disconnect_by_func(win->bookmarks, on_bookmarks_changed, win);
            g_object_unref(win->bookmarks);
            win->bookmarks = NULL;
        }
        /* This is for removing idle_focus_view() */
        if(win->idle_handler)
        {
            g_source_remove(win->idle_handler);
            win->idle_handler = 0;
        }

        all_wins = g_slist_remove(all_wins, win);

        while(gtk_notebook_get_n_pages(win->notebook) > 0)
            gtk_notebook_remove_page(win->notebook, 0);
    }

#if GTK_CHECK_VERSION(3, 0, 0)
    (*GTK_WIDGET_CLASS(fm_main_win_parent_class)->destroy)(object);
#else
    (*GTK_OBJECT_CLASS(fm_main_win_parent_class)->destroy)(object);
#endif
}

static void fm_main_win_finalize(GObject *object)
{
    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_MAIN_WIN(object));

    if (G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)(object);

    pcmanfm_unref();
}

static void on_unrealize(GtkWidget* widget)
{
    int w, h;
    FmMainWin *win = FM_MAIN_WIN(widget);

    gtk_window_get_size(GTK_WINDOW(widget), &w, &h);
    if(!win->fullscreen && !win->maximized &&
       (w != app_config->win_width || h != app_config->win_height))
    {
        app_config->win_width = w;
        app_config->win_height = h;
        pcmanfm_save_config(FALSE);
    }
    if (win->maximized != app_config->maximized)
    {
        app_config->maximized = win->maximized;
        pcmanfm_save_config(FALSE);
    }
    (*GTK_WIDGET_CLASS(fm_main_win_parent_class)->unrealize)(widget);
}

static void on_about_response(GtkDialog* dlg, gint response, GtkAboutDialog **dlgptr)
{
    g_signal_handlers_disconnect_by_func(dlg, on_about_response, dlgptr);
    *dlgptr = NULL;
    pcmanfm_unref();
    gtk_widget_destroy(GTK_WIDGET(dlg));
}

static void on_about(GtkAction* act, FmMainWin* win)
{
    if(!about_dlg)
    {
        GString *comments = g_string_new(_("Lightweight file manager\n"));
#if GTK_CHECK_VERSION(3, 10, 0)
        GtkBuilder* builder = gtk_builder_new_from_file(PACKAGE_UI_DIR "/about.ui");
#else
        GtkBuilder* builder = gtk_builder_new();

        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/about.ui", NULL);
#endif
        about_dlg = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "dlg"));
#if FM_CHECK_VERSION(1, 2, 0)
        g_string_append_printf(comments, _("using LibFM ver. %s\n"), fm_version());
#endif
        g_string_append(comments, _("\nDeveloped by Hon Jen Yee (PCMan)"));
        gtk_about_dialog_set_comments(about_dlg, comments->str);
        g_string_free(comments, TRUE);
        g_object_unref(builder);
        g_signal_connect(about_dlg, "response", G_CALLBACK(on_about_response), (gpointer)&about_dlg);
        pcmanfm_ref();
    }
    gtk_window_present(GTK_WINDOW(about_dlg));
}

static void on_key_nav_list_response(GtkDialog* dlg, gint response, GtkDialog **dlgptr)
{
    g_signal_handlers_disconnect_by_func(dlg, on_key_nav_list_response, dlgptr);
    *dlgptr = NULL;
    gtk_widget_destroy(GTK_WIDGET(dlg));
}

static void on_key_nav_list(GtkAction* act, FmMainWin* win)
{
    if(!key_nav_list_dlg)
    {
        g_debug("creating key_nav_list_dlg");
        key_nav_list_dlg = gtk_message_dialog_new(NULL, 0,
                                                  GTK_MESSAGE_INFO,
                                                  GTK_BUTTONS_OK,
                                                  _("Keyboard Navigation"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(key_nav_list_dlg),
                         _("Tab: cycle focus Folder View -> Side Pane -> Tools Bar\n"
                           "Shift+Tab: cycle focus Tools Bar -> Side Pane -> Folder View\n"
                           "F6: change focus Side pane <-> Folder view\n"
                           "F8: focus divider between Side pane and Folder view\n"
                           "F10: activate main menu\n"
                           "Ctrl+L or Alt+D: jump focus to Path Bar"));
        gtk_window_set_title(GTK_WINDOW(key_nav_list_dlg), _("Keyboard Navigation"));
        g_signal_connect(key_nav_list_dlg, "response", G_CALLBACK(on_key_nav_list_response), (gpointer)&key_nav_list_dlg);
    }
    gtk_window_present(GTK_WINDOW(key_nav_list_dlg));
}

static void on_open_in_terminal(GtkAction* act, FmMainWin* win)
{
    FmPath *path;
#if FM_CHECK_VERSION(1, 0, 2)
    path = fm_nav_history_get_nth_path(win->nav_history,
                                fm_nav_history_get_cur_index(win->nav_history));
    if (path)
#else
    const FmNavHistoryItem* item = fm_nav_history_get_cur(win->nav_history);
    if(item && (path = item->path))
#endif
        pcmanfm_open_folder_in_terminal(GTK_WINDOW(win), path);
}

#if FM_CHECK_VERSION(1, 0, 2)
static void on_search(GtkAction* act, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    FmPath* cwd = fm_tab_page_get_cwd(page);
    GList* l = g_list_append(NULL, cwd);
    fm_launch_search_simple(GTK_WINDOW(win), NULL, l, pcmanfm_open_folder, NULL);
    g_list_free(l);
}
#endif

#if FM_CHECK_VERSION(1, 2, 0)
static void on_launch(GtkAction* act, FmMainWin* win)
{
    char *cmd, *cwd, *def;

    cmd = fm_get_user_input(GTK_WINDOW(win), _("Run a command"),
                            _("Enter a command to run:"), NULL);
    if (cmd == NULL || cmd[0] == '\0') /* cancelled or empty */
        return;
    cwd = fm_path_to_str(fm_tab_page_get_cwd(win->current_page));
    def = g_get_current_dir();
    if (chdir(cwd) != 0) /* error */
    {
        fm_show_error(GTK_WINDOW(win), _("Current folder is inaccessible"), NULL);
    }
    else /* run and return back */
    {
        fm_launch_command_simple(GTK_WINDOW(win), NULL, 0, cmd, NULL);
        /* return back */
        if(chdir(def) != 0)
            g_warning("on_launch(): chdir() failed");
    }
    g_free(cmd);
    g_free(cwd);
    g_free(def);
}
#endif

static void on_show_hidden(GtkToggleAction* act, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    gboolean active;

    if(!page)
        return; /* it's fresh created window, do nothing */

    active = gtk_toggle_action_get_active(act);
    fm_tab_page_set_show_hidden(page, active);
}

static void on_fullscreen(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active = gtk_toggle_action_get_active(act);

    if(win->fullscreen == active)
        return;
    win->fullscreen = active;
    if(win->fullscreen)
    {
        int w, h;

        gtk_window_get_size(GTK_WINDOW(win), &w, &h);
        if(w != app_config->win_width || h != app_config->win_height)
        {
            app_config->win_width = w;
            app_config->win_height = h;
            pcmanfm_save_config(FALSE);
        }
        gtk_window_fullscreen(GTK_WINDOW(win));
    }
    else
        gtk_window_unfullscreen(GTK_WINDOW(win));
}

static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int mode = gtk_radio_action_get_current_value(cur);
    if (win->in_update)
        return;
    fm_standard_view_set_mode(FM_STANDARD_VIEW(win->folder_view), mode);
    if (win->current_page->own_config)
        fm_app_config_save_config_for_path(fm_folder_view_get_cwd(win->folder_view),
                                           win->current_page->sort_type,
                                           win->current_page->sort_by, mode,
                                           win->current_page->show_hidden, NULL);
    else
        win->current_page->view_mode = mode;
}

static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    FmFolderView *fv = win->folder_view;
#if FM_CHECK_VERSION(1, 0, 2)
    FmFolderModel *model = fm_folder_view_get_model(fv);

    if (model)
        fm_folder_model_set_sort(model, val, FM_SORT_DEFAULT);
#else
    fm_folder_view_sort(fv, -1, val);
#endif
    if(val != (int)win->current_page->sort_by)
    {
        win->current_page->sort_by = val;
        if (win->current_page->own_config)
        {
            fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv),
                                               win->current_page->sort_type,
                                               val, -1,
                                               win->current_page->show_hidden,
                                               NULL);
        }
        else
        {
            app_config->sort_by = val;
            pcmanfm_save_config(FALSE);
        }
    }
}

#if FM_CHECK_VERSION(1, 0, 2)
static inline void update_sort_type_for_page(FmTabPage *page, FmFolderView *fv, FmSortMode mode)
#else
static inline void update_sort_type_for_page(FmTabPage *page, FmFolderView *fv, guint mode)
#endif
{
    if(mode != page->sort_type)
    {
        page->sort_type = mode;
        if (page->own_config)
        {
            fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv), mode,
                                               page->sort_by, -1,
                                               page->show_hidden, NULL);
        }
        else
        {
            app_config->sort_type = mode;
            pcmanfm_save_config(FALSE);
        }
    }
}

static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    guint val = gtk_radio_action_get_current_value(cur);
    FmFolderView *fv = win->folder_view;
#if FM_CHECK_VERSION(1, 0, 2)
    FmFolderModel *model = fm_folder_view_get_model(fv);
    FmSortMode mode;

    if (model)
    {
        fm_folder_model_get_sort(model, NULL, &mode);
        mode &= ~FM_SORT_ORDER_MASK;
        mode |= (val == GTK_SORT_ASCENDING) ? FM_SORT_ASCENDING : FM_SORT_DESCENDING;
        fm_folder_model_set_sort(model, -1, mode);
        update_sort_type_for_page(win->current_page, fv, mode);
    }
#else
    fm_folder_view_sort(win->folder_view, val, -1);
    update_sort_type_for_page(win->current_page, fv, val);
#endif
}

#if FM_CHECK_VERSION(1, 2, 0)
static void on_mingle_dirs(GtkToggleAction* act, FmMainWin* win)
{
    FmFolderView *fv = win->folder_view;
    FmFolderModel *model = fm_folder_view_get_model(fv);
    FmSortMode mode;
    gboolean active;

    if (model)
    {
        fm_folder_model_get_sort(model, NULL, &mode);
        active = gtk_toggle_action_get_active(act);
        mode &= ~FM_SORT_NO_FOLDER_FIRST;
        if (active)
            mode |= FM_SORT_NO_FOLDER_FIRST;
        fm_folder_model_set_sort(model, -1, mode);
        update_sort_type_for_page(win->current_page, fv, mode);
    }
}
#endif

#if FM_CHECK_VERSION(1, 0, 2)
static void on_sort_ignore_case(GtkToggleAction* act, FmMainWin* win)
{
    FmFolderView *fv = win->folder_view;
    FmFolderModel *model = fm_folder_view_get_model(fv);
    FmSortMode mode;
    gboolean active;

    if (model)
    {
        fm_folder_model_get_sort(model, NULL, &mode);
        active = gtk_toggle_action_get_active(act);
        mode &= ~FM_SORT_CASE_SENSITIVE;
        if (!active)
            mode |= FM_SORT_CASE_SENSITIVE;
        fm_folder_model_set_sort(model, -1, mode);
        update_sort_type_for_page(win->current_page, fv, mode);
    }
}
#endif

static void on_save_per_folder(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active = gtk_toggle_action_get_active(act);
    FmTabPage *page = win->current_page;
    FmFolderView *fv = win->folder_view;

    if (active)
    {
        if (page->own_config) /* not changed */
            return;
        page->own_config = TRUE;
#if FM_CHECK_VERSION(1, 0, 2)
        page->columns = g_strdupv(app_config->columns);
#endif
        fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv),
                                           page->sort_type, page->sort_by,
                                           fm_standard_view_get_mode(FM_STANDARD_VIEW(fv)),
#if FM_CHECK_VERSION(1, 0, 2)
                                           page->show_hidden, page->columns);
#else
                                           page->show_hidden, NULL);
#endif
    }
    else if (page->own_config) /* attribute removed */
    {
        page->own_config = FALSE;
#if FM_CHECK_VERSION(1, 0, 2)
        g_strfreev(page->columns);
        page->columns = NULL;
#endif
        fm_app_config_clear_config_for_path(fm_folder_view_get_cwd(fv));
    }
}

static void on_side_pane_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    FmTabPage* cur_page = win->current_page;
    FmSidePane* sp;

    if (cur_page == NULL) /* it can be NULL if we are in FmMainWin setup */
        return;
    sp = fm_tab_page_get_side_pane(cur_page);
    int val = gtk_radio_action_get_current_value(cur);
    fm_side_pane_set_mode(sp, val);
}

static gboolean on_focus_in(GtkWidget* w, GdkEventFocus* evt)
{
    if(all_wins->data != w)
    {
        all_wins = g_slist_remove(all_wins, w);
        all_wins = g_slist_prepend(all_wins, w);
    }
    return GTK_WIDGET_CLASS(fm_main_win_parent_class)->focus_in_event(w, evt);
}

static void on_new_win(GtkAction* act, FmMainWin* win)
{
    FmPath* path = fm_tab_page_get_cwd(win->current_page);
    fm_main_win_add_win(win, path);
}

static void on_new_tab(GtkAction* act, FmMainWin* win)
{
    FmPath* path = fm_tab_page_get_cwd(win->current_page);
    fm_main_win_add_tab(win, path);
    /* FR #1967725: focus location bar for newly created tab */
    gtk_window_set_focus(GTK_WINDOW(win), GTK_WIDGET(win->location));
    if (win->idle_handler)
    {
        /* it will steal focus so cancel it */
        g_source_remove(win->idle_handler);
        win->idle_handler = 0;
    }
}

static void on_close_win(GtkAction* act, FmMainWin* win)
{
    gtk_widget_destroy(GTK_WIDGET(win));
}

static void on_close_tab(GtkAction* act, FmMainWin* win)
{
    GtkNotebook* nb = win->notebook;
    /* remove current page */
    gtk_notebook_remove_page(nb, gtk_notebook_get_current_page(nb));
}


static void on_go(GtkAction* act, FmMainWin* win)
{
    /* fm_main_win_chdir_by_name(win, gtk_entry_get_text(GTK_ENTRY(win->location))); */
    g_signal_emit_by_name(win->location, "activate");
}

static void _update_hist_buttons(FmMainWin* win)
{
    GtkAction* act;
    FmNavHistory *nh = fm_tab_page_get_history(win->current_page);

    act = gtk_ui_manager_get_action(win->ui, "/menubar/GoMenu/Next");
#if FM_CHECK_VERSION(1, 0, 2)
    gtk_action_set_sensitive(act, fm_nav_history_get_cur_index(nh) > 0);
#else
    gtk_action_set_sensitive(act, fm_nav_history_can_forward(nh));
#endif
    act = gtk_ui_manager_get_action(win->ui, "/menubar/GoMenu/Prev");
    gtk_action_set_sensitive(act, fm_nav_history_can_back(nh));
    update_file_menu(win, fm_tab_page_get_cwd(win->current_page));
}

static void on_go_back(GtkAction* act, FmMainWin* win)
{
    fm_tab_page_back(win->current_page);
    _update_hist_buttons(win);
}

static void on_go_forward(GtkAction* act, FmMainWin* win)
{
    fm_tab_page_forward(win->current_page);
    _update_hist_buttons(win);
}

static void on_go_up(GtkAction* act, FmMainWin* win)
{
    FmPath* parent = fm_path_get_parent(fm_tab_page_get_cwd(win->current_page));
    if(parent)
        fm_main_win_chdir( win, parent);
}

static void on_go_home(GtkAction* act, FmMainWin* win)
{
#if FM_CHECK_VERSION(1, 2, 0)
    if (app_config->home_path && app_config->home_path[0])
        fm_main_win_chdir_by_name(win, app_config->home_path);
    else
#endif
        fm_main_win_chdir( win, fm_path_get_home());
}

static void on_go_desktop(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir(win, fm_path_get_desktop());
}

static void on_go_trash(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir(win, fm_path_get_trash());
}

static void on_go_computer(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "computer:///");
}

static void on_go_network(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "network:///");
}

static void on_go_apps(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir(win, fm_path_get_apps_menu());
}

static void on_go_connect(GtkAction* act, FmMainWin* win)
{
    open_connect_dialog(GTK_WINDOW(win));
}

void fm_main_win_chdir_by_name(FmMainWin* win, const char* path_str)
{
    FmPath* path = fm_path_new_for_str(path_str);
    fm_main_win_chdir(win, path);
    fm_path_unref(path);
}

void fm_main_win_chdir(FmMainWin* win, FmPath* path)
{
    GtkWidget *current_focus;

    /* NOTE: fm_tab_page_chdir() calls fm_side_pane_chdir(), which can
     * trigger on_side_pane_chdir() callback. So we need to block it here. */
    g_signal_handlers_block_by_func(win->side_pane, on_side_pane_chdir, win);
    fm_tab_page_chdir(win->current_page, path);
    g_signal_handlers_unblock_by_func(win->side_pane, on_side_pane_chdir, win);
    _update_hist_buttons(win);
    update_view_menu(win);
    update_file_menu(win, path);
    /* bug SF#842: if location is focused then set cursor at end of field */
    current_focus = gtk_window_get_focus(GTK_WINDOW(win));
    if (current_focus == (GtkWidget*)win->location)
        gtk_editable_set_position(GTK_EDITABLE(win->location), -1);
}

#if 0
static void close_btn_style_set(GtkWidget *btn, GtkRcStyle *prev, gpointer data)
{
    gint w, h;
    gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
    gtk_widget_set_size_request(btn, w + 2, h + 2);
}
#endif

static gboolean on_tab_label_button_pressed(GtkWidget* tab_label, GdkEventButton* evt, FmTabPage* tab_page)
{
    if(evt->button == 2) /* middle click */
    {
        gtk_widget_destroy(GTK_WIDGET(tab_page));
        return TRUE;
    }
    return FALSE;
}

static void update_statusbar(FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    const char* text;
    if (!app_config->show_statusbar)
        return; /* don't waste time on it */
    gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
    text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_NORMAL);
    if(text)
        gtk_statusbar_push(win->statusbar, win->statusbar_ctx, text);

    gtk_statusbar_pop(win->statusbar, win->statusbar_ctx2);
    text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_SELECTED_FILES);
    if(text)
        gtk_statusbar_push(win->statusbar, win->statusbar_ctx2, text);

    text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_FS_INFO);
    if(text)
    {
        GtkLabel* label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->vol_status)));
        gtk_label_set_text(label, text);
        gtk_widget_show(GTK_WIDGET(win->vol_status));
    }
    else
        gtk_widget_hide(GTK_WIDGET(win->vol_status));
}

gint fm_main_win_add_tab(FmMainWin* win, FmPath* path)
{
    FmTabPage* page = fm_tab_page_new(path);
    GtkWidget* gpage = GTK_WIDGET(page);
    FmTabLabel* label = page->tab_label;
    FmFolderView* folder_view = fm_tab_page_get_folder_view(page);
    gint ret;

    gtk_paned_set_position(GTK_PANED(page), app_config->splitter_pos);

    g_signal_connect(folder_view, "key-press-event", G_CALLBACK(on_view_key_press_event), win);

    g_signal_connect_swapped(label->close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), page);
    g_signal_connect(label, "button-press-event", G_CALLBACK(on_tab_label_button_pressed), page);

    /* add the tab */
    ret = gtk_notebook_append_page(win->notebook, gpage, GTK_WIDGET(page->tab_label));
    gtk_widget_show_all(gpage);
    gtk_notebook_set_tab_reorderable(win->notebook, gpage, TRUE);
    gtk_notebook_set_current_page(win->notebook, ret);

    return ret;
}

static gboolean on_window_state_event(GtkWidget *widget, GdkEventWindowState *evt, FmMainWin *win)
{
    if (evt->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
        win->fullscreen = ((evt->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0);
    if (evt->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
        win->maximized = ((evt->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0);
    return FALSE;
}

FmMainWin* fm_main_win_add_win(FmMainWin* win, FmPath* path)
{
    GtkAction *act;

    win = fm_main_win_new();
    gtk_window_set_default_size(GTK_WINDOW(win),
                                app_config->win_width,
                                app_config->win_height);
    if (app_config->maximized)
        gtk_window_maximize(GTK_WINDOW(win));
    gtk_widget_show_all(GTK_WIDGET(win));
    g_signal_connect(win, "window-state-event", G_CALLBACK(on_window_state_event), win);
    /* create new tab */
    fm_main_win_add_tab(win, path);
    gtk_window_present(GTK_WINDOW(win));
    /* set toolbar visibility and menu toggleables from config */
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ShowToolbar");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.visible);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNewWin");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.new_win);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNewTab");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.new_tab);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarNav");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.nav);
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Toolbar/ToolbarHome");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->tb.home);
    /* the same for statusbar */
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/ShowStatus");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), app_config->show_statusbar);
    gtk_widget_set_visible(GTK_WIDGET(win->statusbar), app_config->show_statusbar);
    /* the same for path bar mode */
    gtk_widget_hide(GTK_WIDGET(win->path_bar));
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/PathMode/PathEntry");
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), app_config->pathbar_mode_buttons);
    return win;
}

static void on_open(GtkAction* act, FmMainWin* win)
{
    FmFileInfoList *files = fm_folder_view_dup_selected_files(win->folder_view);
    if(files)
    {
        GList* l = fm_file_info_list_peek_head_link(files);
        if (g_list_length(l) > 0)
            fm_launch_files_simple(GTK_WINDOW(win), NULL, l, pcmanfm_open_folder, NULL);
        fm_file_info_list_unref(files);
    }
}

static void on_link(GtkAction *act, FmMainWin *win)
{
    FmPathList *files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if (files)
    {
        FmPath *dest = fm_select_folder(GTK_WINDOW(win), NULL);
        if (dest)
        {
            fm_link_files(GTK_WINDOW(win), files, dest);
            fm_path_unref(dest);
        }
        fm_path_list_unref(files);
    }
}

static void on_copy_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_copy_files_to(GTK_WINDOW(win), files);
        fm_path_list_unref(files);
    }
}

static void on_move_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_move_files_to(GTK_WINDOW(win), files);
        fm_path_list_unref(files);
    }
}

static void on_rename(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_rename_file(GTK_WINDOW(win), fm_path_list_peek_head(files));
        /* FIXME: is it ok to only rename the first selected file here? */
        fm_path_list_unref(files);
    }
}

static void on_trash(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_trash_files(GTK_WINDOW(win), files);
        fm_path_list_unref(files);
    }
}

static void on_del(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_delete_files(GTK_WINDOW(win), files);
        fm_path_list_unref(files);
    }
}

static void on_copy_path(GtkAction* action, FmMainWin* win)
{
    GdkDisplay *dpy = gtk_widget_get_display(GTK_WIDGET(win));
    GtkClipboard *clipboard = gtk_clipboard_get_for_display(dpy, GDK_SELECTION_CLIPBOARD);
    GString *str = g_string_sized_new(128);
    FmPathList *files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    GList *fl;

    for (fl = fm_path_list_peek_head_link(files); fl; fl = fl->next)
    {
        char *path = fm_path_to_str(fl->data);
        if (str->len > 0)
            g_string_append_c(str, '\n');
        g_string_append(str, path);
        g_free(path);
    }
    gtk_clipboard_set_text(clipboard, str->str, str->len);
    g_string_free(str, TRUE);
}

static void on_preference(GtkAction* act, FmMainWin* win)
{
    fm_edit_preference(GTK_WINDOW(win), 0);
}

static void on_add_bookmark(GtkAction* act, FmMainWin* win)
{
    FmPath* cwd = fm_tab_page_get_cwd(win->current_page);
    char* disp_path = fm_path_display_name(cwd, TRUE);
    char* msg = g_strdup_printf(_("Add following folder to bookmarks:\n\'%s\'\nEnter a name for the new bookmark item:"), disp_path);
    char* disp_name = fm_path_display_basename(cwd);
    char* name;
    g_free(disp_path);
    name = fm_get_user_input(GTK_WINDOW(win), _("Add to Bookmarks"), msg, disp_name);
    g_free(disp_name);
    g_free(msg);
    if(name)
    {
        fm_bookmarks_append(win->bookmarks, cwd, name);
        g_free(name);
    }
}

static void on_show_toolbar(GtkToggleAction *action, FmMainWin *win)
{
    gboolean active = gtk_toggle_action_get_active(action);

    app_config->tb.visible = active;
    fm_config_emit_changed(fm_config, "toolsbar");
    pcmanfm_save_config(FALSE);
}

/* toolbar items: NewWin NewTab Prev (Hist) Next Up Home (Location) Go */
static void on_toolbar_new_win(GtkToggleAction *act, FmMainWin *win)
{
    gboolean active = gtk_toggle_action_get_active(act);

    app_config->tb.new_win = active;
    fm_config_emit_changed(fm_config, "toolsbar");
    pcmanfm_save_config(FALSE);
}

static void on_toolbar_new_tab(GtkToggleAction *act, FmMainWin *win)
{
    gboolean active = gtk_toggle_action_get_active(act);

    app_config->tb.new_tab = active;
    fm_config_emit_changed(fm_config, "toolsbar");
    pcmanfm_save_config(FALSE);
}

static void on_toolbar_nav(GtkToggleAction *act, FmMainWin *win)
{
    gboolean active = gtk_toggle_action_get_active(act);

    app_config->tb.nav = active;
    fm_config_emit_changed(fm_config, "toolsbar");
    pcmanfm_save_config(FALSE);
}

static void on_toolbar_home(GtkToggleAction *act, FmMainWin *win)
{
    gboolean active = gtk_toggle_action_get_active(act);

    app_config->tb.home = active;
    fm_config_emit_changed(fm_config, "toolsbar");
    pcmanfm_save_config(FALSE);
}

static void on_location(GtkAction* act, FmMainWin* win)
{
    gtk_widget_grab_focus(GTK_WIDGET(win->location));
    if (app_config->pathbar_mode_buttons)
    {
        gtk_widget_show(GTK_WIDGET(win->location));
        gtk_widget_show(gtk_ui_manager_get_widget(win->ui, "/toolbar/Go"));
        gtk_widget_hide(GTK_WIDGET(win->path_bar));
    }
}

static void bounce_action(GtkAction* act, FmMainWin* win)
{
    GtkWindow *window = GTK_WINDOW(win);
    GtkWidget *current_focus;

    g_debug("bouncing action %s to popup", gtk_action_get_name(act));
    /* save current focus */
    current_focus = gtk_window_get_focus(window);
    /* bug #3615003: if folder view does not have the focus, action will not work */
    gtk_window_set_focus(window, GTK_WIDGET(win->folder_view));
    fm_folder_view_bounce_action(act, win->folder_view);
    /* restore focus back */
    gtk_window_set_focus(window, current_focus);
}

static guint icon_sizes[] =
{
    8,
    10,
    12, /* 12.6 */
    16,
    20,
    24, /* 25.2 */
    32,
    40,
    48, /* 50.4 */
    64,
    80,
    96, /* 101 */
    128, /* 127 */
    160,
    192, /* 202 */
    256, /* 254 */
    320,
    400, /* 403 */
    512
};

static void on_size_decrement(GtkAction *act, FmMainWin *win)
{
    FmStandardViewMode mode;
    guint size, i;

    if (win->folder_view == NULL)
        return;
    mode = fm_standard_view_get_mode(FM_STANDARD_VIEW(win->folder_view));
    switch (mode)
    {
    case FM_FV_ICON_VIEW:
        size = fm_config->big_icon_size;
        if (size < 24)
            return;
        break;
    case FM_FV_COMPACT_VIEW:
    case FM_FV_LIST_VIEW:
        size = fm_config->small_icon_size;
        if (size < 10)
            return;
        break;
    case FM_FV_THUMBNAIL_VIEW:
        size = fm_config->thumbnail_size;
        if (size < 64)
            return;
        break;
    default:
        return;
    }
    for (i = G_N_ELEMENTS(icon_sizes); i > 0; )
    {
        i--;
        if (icon_sizes[i] < size)
            break;
    }
    switch (mode)
    {
    case FM_FV_ICON_VIEW:
        fm_config->big_icon_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "big_icon_size");
        break;
    case FM_FV_COMPACT_VIEW:
    case FM_FV_LIST_VIEW:
        fm_config->small_icon_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "small_icon_size");
        break;
    case FM_FV_THUMBNAIL_VIEW:
        fm_config->thumbnail_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "thumbnail_size");
    }
    pcmanfm_save_config(FALSE);
}

static void on_size_increment(GtkAction *act, FmMainWin *win)
{
    FmStandardViewMode mode;
    guint size, i;

    if (win->folder_view == NULL)
        return;
    mode = fm_standard_view_get_mode(FM_STANDARD_VIEW(win->folder_view));
    switch (mode)
    {
    case FM_FV_ICON_VIEW:
        size = fm_config->big_icon_size;
        if (size > 200)
            return;
        break;
    case FM_FV_COMPACT_VIEW:
    case FM_FV_LIST_VIEW:
        size = fm_config->small_icon_size;
        if (size > 100)
            return;
        break;
    case FM_FV_THUMBNAIL_VIEW:
        size = fm_config->thumbnail_size;
        if (size > 400)
            return;
        break;
    default:
        return;
    }
    for (i = 1; i < G_N_ELEMENTS(icon_sizes); i++)
    {
        if (icon_sizes[i] > size)
            break;
    }
    switch (mode)
    {
    case FM_FV_ICON_VIEW:
        fm_config->big_icon_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "big_icon_size");
        break;
    case FM_FV_COMPACT_VIEW:
    case FM_FV_LIST_VIEW:
        fm_config->small_icon_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "small_icon_size");
        break;
    case FM_FV_THUMBNAIL_VIEW:
        fm_config->thumbnail_size = icon_sizes[i];
        fm_config_emit_changed(fm_config, "thumbnail_size");
    }
    pcmanfm_save_config(FALSE);
}

static void on_size_default(GtkAction *act, FmMainWin *win)
{
    FmStandardViewMode mode;

    if (win->folder_view == NULL)
        return;
    mode = fm_standard_view_get_mode(FM_STANDARD_VIEW(win->folder_view));
    switch (mode)
    {
    case FM_FV_ICON_VIEW:
        if (fm_config->big_icon_size == 48)
            return;
        fm_config->big_icon_size = 48;
        fm_config_emit_changed(fm_config, "big_icon_size");
        break;
    case FM_FV_COMPACT_VIEW:
    case FM_FV_LIST_VIEW:
        if (fm_config->small_icon_size == 20)
            return;
        fm_config->small_icon_size = 20;
        fm_config_emit_changed(fm_config, "small_icon_size");
        break;
    case FM_FV_THUMBNAIL_VIEW:
        if (fm_config->thumbnail_size == 128)
            return;
        fm_config->thumbnail_size = 128;
        fm_config_emit_changed(fm_config, "thumbnail_size");
        break;
    default:
        return;
    }
    pcmanfm_save_config(FALSE);
}

/* This callback is only connected to folder view of current active tab page. */
static void on_folder_view_clicked(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* fi, FmMainWin* win)
{
    if(fv != win->folder_view)
        return;

    switch(type)
    {
    case FM_FV_ACTIVATED: /* file activated */
        break; /* handled by FmFolderView */
    case FM_FV_CONTEXT_MENU:
        break; /* handled by FmFolderView */
    case FM_FV_MIDDLE_CLICK:
        if(fi && fm_file_info_is_dir(fi))
            fm_main_win_add_tab(win, fm_file_info_get_path(fi));
        break;
    case FM_FV_CLICK_NONE: ;
    }
}

/* This callback is only connected to current active tab page. */
static void on_tab_page_status_text(FmTabPage* page, guint type, const char* status_text, FmMainWin* win)
{
    if(page != win->current_page)
        return;
    if (!app_config->show_statusbar)
        return;

    switch(type)
    {
    case FM_STATUS_TEXT_NORMAL:
        gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
        if(status_text)
            gtk_statusbar_push(win->statusbar, win->statusbar_ctx, status_text);
        else
            break; /* no updates are required */
        if (fm_folder_view_get_n_selected_files(win->folder_view) == 0)
            break;
        /* otherwise put selection info on statusbar, continue with update */
        status_text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_SELECTED_FILES);
    case FM_STATUS_TEXT_SELECTED_FILES:
        gtk_statusbar_pop(win->statusbar, win->statusbar_ctx2);
        if(status_text)
            gtk_statusbar_push(win->statusbar, win->statusbar_ctx2, status_text);
        break;
    case FM_STATUS_TEXT_FS_INFO:
        if(status_text)
        {
            GtkLabel* label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(win->vol_status)));
            gtk_label_set_text(label, status_text);
            gtk_widget_show(GTK_WIDGET(win->vol_status));
        }
        else
            gtk_widget_hide(GTK_WIDGET(win->vol_status));
        break;
    }
}

static void on_tab_page_chdir(FmTabPage* page, FmPath* path, FmMainWin* win)
{
    if(page != win->current_page)
        return;

    fm_path_entry_set_path(win->location, path);
    fm_path_bar_set_path(win->path_bar, path);
    gtk_window_set_title(GTK_WINDOW(win), fm_tab_page_get_title(page));
}

static void on_tab_page_got_focus(FmTabPage* page, FmMainWin* win)
{
    int n = gtk_notebook_page_num(win->notebook, GTK_WIDGET(page));
    /* sometimes views receive focus while they are in rendering process
       therefore click on tab will not always work as expected because it
       is reset in this callback, the trick is if notebook is in update
       process then focus change is pending (see on_notebook_switch_page()
       for details) therefore we can check that and do nothing */
    if (n >= 0 && win->idle_handler == 0) /* don't change page if it is pending */
        gtk_notebook_set_current_page(win->notebook, n);
}

/* this callback may be called only after tab page is added */
static void on_tab_page_loaded(FmTabPage *page, FmMainWin *win)
{
    FmFolderView *folder_view = fm_tab_page_get_folder_view(page);

    /* update folder popup */
    fm_folder_view_set_active(folder_view, FALSE);
    fm_folder_view_add_popup(folder_view, GTK_WINDOW(win), NULL);
    fm_folder_view_set_active(folder_view, (page == win->current_page));
}

#if FM_CHECK_VERSION(1, 0, 2)
static void on_folder_view_filter_changed(FmFolderView* fv, FmMainWin* win)
{
    GtkAction* act;
    gboolean active;

    if (fv != win->folder_view)
        return;

    active = fm_folder_view_get_show_hidden(fv);

    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/ShowHidden");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), active);
    if(active != win->current_page->show_hidden)
    {
        win->current_page->show_hidden = active;
        if (win->current_page->own_config)
        {
            fm_app_config_save_config_for_path(fm_folder_view_get_cwd(fv),
                                               win->current_page->sort_type,
                                               win->current_page->sort_by, -1,
                                               active, NULL);
        }
        else
        {
            app_config->show_hidden = active;
            pcmanfm_save_config(FALSE);
        }
    }
}
#endif

static inline FmTabPage *_find_tab_page(FmMainWin *win, FmFolderView *fv)
{
    FmTabPage *page;
    gint n;

    for (n = gtk_notebook_get_n_pages(win->notebook); n > 0; )
    {
        page = FM_TAB_PAGE(gtk_notebook_get_nth_page(win->notebook, --n));
        if (fv == fm_tab_page_get_folder_view(page))
            return page;
    }
    return NULL;
}

static void on_notebook_switch_page(GtkNotebook* nb, gpointer* new_page, guint num, FmMainWin* win)
{
    GtkWidget* sw_page = gtk_notebook_get_nth_page(nb, num);
    FmTabPage* page;
    FmFolderView *old_view = NULL, *passive_view = NULL;

    g_return_if_fail(FM_IS_TAB_PAGE(sw_page));
    page = (FmTabPage*)sw_page;
    /* deactivate gestures from old view first */
    if(win->folder_view)
    {
        fm_folder_view_set_active(win->folder_view, FALSE);
        g_object_unref(win->folder_view);
    }

    /* remember old views for checks below */
    if (win->current_page)
    {
        passive_view = fm_tab_page_get_passive_view(win->current_page);
        old_view = fm_tab_page_get_folder_view(win->current_page);
    }

    /* connect to the new active page */
    win->current_page = page;
    win->folder_view = fm_tab_page_get_folder_view(page);
    if(win->folder_view)
        g_object_ref(win->folder_view);
    win->nav_history = fm_tab_page_get_history(page);
    win->side_pane = fm_tab_page_get_side_pane(page);

    /* set active and passive panes */
    if (win->enable_passive_view && passive_view)
    {
        FmTabPage *psv_page;

        if (win->folder_view == passive_view)
        {
            /* we moved to other pane so we have to set passive_view_on_right */
            win->passive_view_on_right = win->passive_view_on_right ? FALSE : TRUE;
            passive_view = old_view;
        }
        /* now set it */
        fm_tab_page_set_passive_view(page, passive_view, win->passive_view_on_right);
        /* FIXME: log errors */
        /* ok, passive view was just changed, we have to update the button */
        psv_page = _find_tab_page(win, passive_view);
        if (psv_page)
            gtk_widget_set_state(GTK_WIDGET(psv_page->tab_label), GTK_STATE_SELECTED);
    }
    else
    {
        /* FIXME: log errors */
        fm_tab_page_take_view_back(page);
    }

    /* reactivate gestures */
    fm_folder_view_set_active(win->folder_view, TRUE);
    g_debug("reactivated gestures to page %u", num);
    /* update Cut/Copy/Del status */
    on_folder_view_sel_changed(win->folder_view,
                               fm_folder_view_get_n_selected_files(win->folder_view),
                               win);
    _update_hist_buttons(win);
#if FM_CHECK_VERSION(1, 0, 2)
    on_folder_view_filter_changed(win->folder_view, win);
#endif

    /* update side pane state */
    if(app_config->side_pane_mode & FM_SP_HIDE) /* hidden */
    {
        gtk_widget_hide(GTK_WIDGET(win->side_pane));
    }
    else
    {
        fm_side_pane_set_mode(win->side_pane,
                              (app_config->side_pane_mode & FM_SP_MODE_MASK));
        gtk_widget_show_all(GTK_WIDGET(win->side_pane));
    }

    fm_path_entry_set_path(win->location, fm_tab_page_get_cwd(page));
    fm_path_bar_set_path(win->path_bar, fm_tab_page_get_cwd(page));
    gtk_window_set_title((GtkWindow*)win, fm_tab_page_get_title(page));

    update_sort_menu(win);
    update_view_menu(win);
    update_file_menu(win, fm_tab_page_get_cwd(page));
    update_statusbar(win);

    if(win->idle_handler == 0)
        win->idle_handler = gdk_threads_add_idle_full(G_PRIORITY_LOW,
                                                      idle_focus_view, win, NULL);
}

static void on_notebook_page_added(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win)
{
    FmTabPage* tab_page = FM_TAB_PAGE(page);

    g_signal_connect(tab_page, "notify::position",
                     G_CALLBACK(on_tab_page_splitter_pos_changed), win);
    g_signal_connect(tab_page, "chdir",
                     G_CALLBACK(on_tab_page_chdir), win);
    g_signal_connect(tab_page, "status",
                     G_CALLBACK(on_tab_page_status_text), win);
    g_signal_connect(tab_page, "got-focus",
                     G_CALLBACK(on_tab_page_got_focus), win);
    g_signal_connect(tab_page, "loaded",
                     G_CALLBACK(on_tab_page_loaded), win);
    /* FIXME: remove direct access */
    g_signal_connect(tab_page->folder_view, "sort-changed",
                     G_CALLBACK(on_folder_view_sort_changed), win);
    g_signal_connect(tab_page->folder_view, "sel-changed",
                     G_CALLBACK(on_folder_view_sel_changed), win);
#if FM_CHECK_VERSION(1, 0, 2)
    /* connect to "filter-changed" to get ShowHidden state */
    g_signal_connect(tab_page->folder_view, "filter-changed",
                     G_CALLBACK(on_folder_view_filter_changed), win);
#endif
    g_signal_connect(tab_page->folder_view, "clicked",
                     G_CALLBACK(on_folder_view_clicked), win);
    g_signal_connect(tab_page->side_pane, "mode-changed",
                     G_CALLBACK(on_side_pane_mode_changed), win);
    g_signal_connect(tab_page->side_pane, "chdir",
                     G_CALLBACK(on_side_pane_chdir), win);

    /* create folder popup and apply shortcuts from it */
    fm_folder_view_add_popup(tab_page->folder_view, GTK_WINDOW(win), NULL);
    /* disable it yet - it will be enabled in on_notebook_switch_page() */
    fm_folder_view_set_active(tab_page->folder_view, FALSE);

    if(gtk_notebook_get_n_pages(nb) > 1
       || app_config->always_show_tabs)
        gtk_notebook_set_show_tabs(nb, TRUE);
    else
        gtk_notebook_set_show_tabs(nb, FALSE);
}


static void on_notebook_page_removed(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win)
{
    FmTabPage* tab_page = FM_TAB_PAGE(page);
    FmFolderView* folder_view = fm_tab_page_get_folder_view(tab_page);
    GtkAction* act;

    g_debug("FmMainWin[%p]: removed page %u[%p](view=%p); %u pages left", win,
            num, page, folder_view, gtk_notebook_get_n_pages(nb));
    /* disconnect from previous active page */
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_splitter_pos_changed, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_chdir, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_status_text, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_got_focus, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_loaded, win);
    if(folder_view)
    {
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_view_key_press_event, win);
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_folder_view_sort_changed, win);
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_folder_view_sel_changed, win);
#if FM_CHECK_VERSION(1, 0, 2)
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_folder_view_filter_changed, win);
#endif
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_folder_view_clicked, win);
        /* update menu if passive pane was removed */
        if (win->current_page && win->enable_passive_view &&
            fm_tab_page_get_passive_view(win->current_page) == folder_view)
        {
            win->enable_passive_view = FALSE;
            act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/DualPane");
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), FALSE);
        }
    }
    g_signal_handlers_disconnect_by_func(tab_page->side_pane,
                                         on_side_pane_mode_changed, win);
    g_signal_handlers_disconnect_by_func(tab_page->side_pane,
                                         on_side_pane_chdir, win);

    if(tab_page == win->current_page)
    {
        win->current_page = NULL;
        if (win->folder_view)
            g_object_unref(win->folder_view);
        win->folder_view = NULL;
        win->nav_history = NULL;
        win->side_pane = NULL;
    }

    if(gtk_notebook_get_n_pages(nb) > 1 || app_config->always_show_tabs)
        gtk_notebook_set_show_tabs(nb, TRUE);
    else
        gtk_notebook_set_show_tabs(nb, FALSE);

    /* all notebook pages are removed, let's destroy the main window */
    if(gtk_notebook_get_n_pages(nb) == 0)
        gtk_widget_destroy(GTK_WIDGET(win));
}

FmMainWin* fm_main_win_get_last_active(void)
{
    return all_wins ? (FmMainWin*)all_wins->data : NULL;
}

void fm_main_win_open_in_last_active(FmPath* path)
{
    FmMainWin* win = fm_main_win_get_last_active();
    if(!win)
        win = fm_main_win_add_win(NULL, path);
    else
        fm_main_win_add_tab(win, path);
    gtk_window_present(GTK_WINDOW(win));
}

static void switch_to_next_tab(FmMainWin* win)
{
    int n = gtk_notebook_get_current_page(win->notebook);
    if(n < gtk_notebook_get_n_pages(win->notebook) - 1)
        ++n;
    else
        n = 0;
    gtk_notebook_set_current_page(win->notebook, n);
}

static void switch_to_prev_tab(FmMainWin* win)
{
    int n = gtk_notebook_get_current_page(win->notebook);
    if(n > 0)
        --n;
    else
        n = gtk_notebook_get_n_pages(win->notebook) - 1;
    gtk_notebook_set_current_page(win->notebook, n);
}

static gboolean on_key_press_event(GtkWidget* w, GdkEventKey* evt)
{
    FmMainWin* win = FM_MAIN_WIN(w);
    int modifier = evt->state & gtk_accelerator_get_default_mod_mask();

    if(modifier == GDK_MOD1_MASK) /* Alt */
    {
        if(isdigit(evt->keyval)) /* Alt + 0 ~ 9, nth tab */
        {
            int n;
            if(evt->keyval == '0')
                n = 9;
            else
                n = evt->keyval - '1';
            gtk_notebook_set_current_page(win->notebook, n);
            return TRUE;
        }
    }
    else if(modifier == GDK_CONTROL_MASK) /* Ctrl */
    {
        if(evt->keyval == GDK_KEY_Tab
         || evt->keyval == GDK_KEY_ISO_Left_Tab
         || evt->keyval == GDK_KEY_Page_Down) /* Ctrl + Tab or PageDown, next tab */
        {
            switch_to_next_tab(win);
            return TRUE;
        }
        else if(evt->keyval == GDK_KEY_Page_Up)
        {
            switch_to_prev_tab(win);
            return TRUE;
        }
    }
    else if(modifier == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) /* Ctrl + Shift */
    {
        if(evt->keyval == GDK_KEY_Tab
         || evt->keyval == GDK_KEY_ISO_Left_Tab) /* Ctrl + Shift + Tab or PageUp, previous tab */
        {
            switch_to_prev_tab(win);
            return TRUE;
        }
    }
    else if(evt->keyval == '/' || evt->keyval == '~')
    {
        if (!gtk_widget_is_focus(GTK_WIDGET(win->location)))
        {
            gtk_widget_grab_focus(GTK_WIDGET(win->location));
            char path[] = {evt->keyval, 0};
            gtk_entry_set_text(GTK_ENTRY(win->location), path);
            gtk_editable_set_position(GTK_EDITABLE(win->location), -1);
            return TRUE;
        }
    }
    else if(evt->keyval == GDK_KEY_Escape)
    {
        if (gtk_widget_is_focus(GTK_WIDGET(win->location)))
        {
            gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
            fm_path_entry_set_path(win->location,
                                   fm_tab_page_get_cwd(win->current_page));
            fm_path_bar_set_path(win->path_bar,
                                 fm_tab_page_get_cwd(win->current_page));
            return TRUE;
        }
    }
    return GTK_WIDGET_CLASS(fm_main_win_parent_class)->key_press_event(w, evt);
}

static gboolean on_button_press_event(GtkWidget* w, GdkEventButton* evt)
{
    FmMainWin* win = FM_MAIN_WIN(w);
    GtkAction* act;

    if(evt->button == 8) /* back */
    {
        act = gtk_ui_manager_get_action(win->ui, "/Prev2");
        gtk_action_activate(act);
    }
    else if(evt->button == 9) /* forward */
    {
        act = gtk_ui_manager_get_action(win->ui, "/Next2");
        gtk_action_activate(act);
    }
    if(GTK_WIDGET_CLASS(fm_main_win_parent_class)->button_press_event)
        return GTK_WIDGET_CLASS(fm_main_win_parent_class)->button_press_event(w, evt);
    else
        return FALSE;
}

static gboolean on_scroll_event(GtkWidget* w, GdkEventScroll* evt)
{
    FmMainWin* win = FM_MAIN_WIN(w);
    GtkAction* act;
    int modifier = evt->state & gtk_accelerator_get_default_mod_mask();

    /* g_debug("on_scroll_event:%d", evt->direction); */
    if (modifier != GDK_CONTROL_MASK) ;
    else if(evt->direction == GDK_SCROLL_UP)
    {
        act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/SizeBigger");
        gtk_action_activate(act);
        return TRUE;
    }
    else if(evt->direction == GDK_SCROLL_DOWN)
    {
        act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/SizeSmaller");
        gtk_action_activate(act);
        return TRUE;
    }
    if(GTK_WIDGET_CLASS(fm_main_win_parent_class)->scroll_event)
        return GTK_WIDGET_CLASS(fm_main_win_parent_class)->scroll_event(w, evt);
    else
        return FALSE;
}

static void on_reload(GtkAction* act, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    fm_tab_page_reload(page);
}

#if FM_CHECK_VERSION(1, 0, 2)
static void on_filter(GtkAction* act, FmMainWin* win)
{
    FmTabPage *page = win->current_page;
    const char *old_filter = page->filter_pattern ? page->filter_pattern : "*";
    char *new_filter;

    new_filter = fm_get_user_input(GTK_WINDOW(win), _("Select filter"),
                                   _("Choose a new shell pattern to show files:"),
                                   old_filter);
    if (!new_filter) /* cancelled */
        return;
    if (strcmp(new_filter, "*"))
        fm_tab_page_set_filter_pattern(page, new_filter);
    else
        fm_tab_page_set_filter_pattern(page, NULL);
    g_free(new_filter);
    gtk_window_set_title(GTK_WINDOW(win), fm_tab_page_get_title(page));
}
#endif

static void on_show_side_pane(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active;

    active = gtk_toggle_action_get_active(act);
    if(active)
    {
        app_config->side_pane_mode &= ~FM_SP_HIDE;
        gtk_widget_show_all(GTK_WIDGET(win->side_pane));
    }
    else
    {
        app_config->side_pane_mode |= FM_SP_HIDE;
        gtk_widget_hide(GTK_WIDGET(win->side_pane));
    }
    /* FIXME: propagate the event to other windows? */
    pcmanfm_save_config(FALSE);
}

static void on_dual_pane(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active;
    int num;
    FmFolderView *fv;
    GtkWidget *page;

    active = gtk_toggle_action_get_active(act);
    /* g_debug("on_dual_pane: %d", active); */
    if (active && !win->enable_passive_view)
    {
        num = gtk_notebook_get_n_pages(win->notebook);
        if (num < 2) /* single page yet */
        {
            win->passive_view_on_right = FALSE;
            num = fm_main_win_add_tab(win, fm_tab_page_get_cwd(win->current_page));
            g_debug("on_dual_pane: added duplicate of the single page");
            page = gtk_notebook_get_nth_page(win->notebook, 0);
        }
        else if (gtk_notebook_get_current_page(win->notebook) < num - 1)
        {
            /* there is a page on right */
            win->passive_view_on_right = TRUE;
            num = gtk_notebook_get_current_page(win->notebook) + 1;
            g_debug("on_dual_pane: adding passive page %d to right pane", num);
            page = gtk_notebook_get_nth_page(win->notebook, num);
        }
        else
        {
            /* it is the most right page */
            win->passive_view_on_right = FALSE;
            g_debug("on_dual_pane: adding passive page %d to left pane", num - 2);
            page = gtk_notebook_get_nth_page(win->notebook, num - 2);
        }
        fv = fm_tab_page_get_folder_view(FM_TAB_PAGE(page));
        fm_tab_page_set_passive_view(win->current_page, fv,
                                     win->passive_view_on_right);
        /* ok, passive view was just changed, we have to update the button */
        gtk_widget_set_state(GTK_WIDGET(FM_TAB_PAGE(page)->tab_label), GTK_STATE_SELECTED);
        win->enable_passive_view = TRUE;
    }
    else if (!active && win->enable_passive_view)
    {
        /* take passive pane back to the tab page */
        fv = fm_tab_page_get_passive_view(win->current_page);
        if (fv)
        {
            FmTabPage *tp = _find_tab_page(win, fv);
            if (tp)
            {
                fm_tab_page_take_view_back(tp);
                gtk_widget_set_state(GTK_WIDGET(FM_TAB_PAGE(tp)->tab_label), GTK_STATE_ACTIVE);
            }
        }
        win->enable_passive_view = FALSE;
    }
}

static void on_show_status(GtkToggleAction *action, FmMainWin *win)
{
    gboolean active;

    active = gtk_toggle_action_get_active(action);
    if (active != app_config->show_statusbar)
    {
        app_config->show_statusbar = active;
        fm_config_emit_changed(fm_config, "statusbar");
        pcmanfm_save_config(FALSE);
    }
}
