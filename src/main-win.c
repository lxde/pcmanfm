/*
 *      main-win.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *      Copyright 2012 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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
static void on_unrealize(GtkWidget* widget);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_new_tab(GtkAction* act, FmMainWin* win);
static void on_close_tab(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);

static void on_open_in_new_tab(GtkAction* act, FmMainWin* win);
static void on_open_in_new_win(GtkAction* act, FmMainWin* win);

static void on_cut(GtkAction* act, FmMainWin* win);
static void on_copy(GtkAction* act, FmMainWin* win);
static void on_copy_to(GtkAction* act, FmMainWin* win);
static void on_move_to(GtkAction* act, FmMainWin* win);
static void on_paste(GtkAction* act, FmMainWin* win);
static void on_del(GtkAction* act, FmMainWin* win);
static void on_rename(GtkAction* act, FmMainWin* win);

static void on_select_all(GtkAction* act, FmMainWin* win);
static void on_invert_select(GtkAction* act, FmMainWin* win);
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
static void on_reload(GtkAction* act, FmMainWin* win);
static void on_show_hidden(GtkToggleAction* act, FmMainWin* win);
static void on_show_side_pane(GtkToggleAction* act, FmMainWin* win);
static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_side_pane_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);
static void on_open_folder_in_terminal(GtkAction* act, FmMainWin* win);
static void on_open_in_terminal(GtkAction* act, FmMainWin* win);
static void on_open_as_root(GtkAction* act, FmMainWin* win);

static void on_location(GtkAction* act, FmMainWin* win);

static void on_create_new(GtkAction* action, FmMainWin* win);
static void on_prop(GtkAction* action, FmMainWin* win);

static void on_notebook_switch_page(GtkNotebook* nb, GtkNotebookPage* page, guint num, FmMainWin* win);
static void on_notebook_page_added(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win);
static void on_notebook_page_removed(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win);

static void on_folder_view_clicked(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* fi, FmMainWin* win);

#include "main-win-ui.c" /* ui xml definitions and actions */

static GSList* all_wins = NULL;
static GtkDialog* about_dlg = NULL;

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
    widget_class->unrealize = on_unrealize;

    fm_main_win_parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);
}

static gboolean idle_focus_view(gpointer user_data)
{
    FmMainWin* win = (FmMainWin*)user_data;
    if(win->folder_view)
        gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
    return FALSE;
}

static void on_location_activate(GtkEntry* entry, FmMainWin* win)
{
    FmPath* path = fm_path_entry_get_path(FM_PATH_ENTRY(entry));
    fm_main_win_chdir(win, path);

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
        win->idle_handler = g_idle_add_full(G_PRIORITY_LOW, idle_focus_view, win, NULL);
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

static FmJobErrorAction on_query_target_info_error(FmJob* job, GError* err, FmJobErrorSeverity severity, FmMainWin* win)
{
    if(err->domain == G_IO_ERROR)
    {
        if(err->code == G_IO_ERROR_NOT_MOUNTED)
        {
            if(fm_mount_path(GTK_WINDOW(win), fm_file_info_job_get_current(FM_FILE_INFO_JOB(job)), TRUE))
                return FM_JOB_RETRY;
        }
        else if(err->code == G_IO_ERROR_FAILED_HANDLED)
            return FM_JOB_CONTINUE;
    }
    fm_show_error(GTK_WINDOW(win), NULL, err->message);
    return FM_JOB_CONTINUE;
}

static void update_sort_menu(FmMainWin* win)
{
    GtkAction* act;
    FmFolderView* fv = win->folder_view;
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/Asc");
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), fm_folder_view_get_sort_type(fv));
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/Sort/ByName");
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), fm_folder_view_get_sort_by(fv));
}

static void update_view_menu(FmMainWin* win)
{
    GtkAction* act;
    FmFolderView* fv = win->folder_view;
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/ShowHidden");
    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(act), fm_folder_view_get_show_hidden(fv));
    act = gtk_ui_manager_get_action(win->ui, "/menubar/ViewMenu/IconView");
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(act), fm_folder_view_get_mode(fv));
}

static void on_folder_view_sort_changed(FmFolderView* fv, FmMainWin* win)
{
    if(fv != win->folder_view)
        return;
    update_sort_menu(win);
}

static gboolean on_view_key_press_event(FmFolderView* fv, GdkEventKey* evt, FmMainWin* win)
{
    switch(evt->keyval)
    {
    case GDK_BackSpace:
        on_go_up(NULL, win);
        break;
    case GDK_Delete:
        on_del(NULL, win);
        break;
    case GDK_Menu:
        {
            FmFileInfoList *files = fm_folder_view_dup_selected_files(fv);
            FmFileInfo *info;
            if(files && !fm_file_info_list_is_empty(files))
                info = fm_file_info_list_peek_head(files);
            else
                info = NULL;
            on_folder_view_clicked(fv, FM_FV_CONTEXT_MENU, info, win);
            if(files)
                fm_file_info_list_unref(files);
            break;
        }
    }
    return FALSE;
}

static void on_bookmark(GtkMenuItem* mi, FmMainWin* win)
{
    FmPath* path = (FmPath*)g_object_get_data(G_OBJECT(mi), "path");
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
    GList* l;
    GtkWidget* mi;
    int i = 0;

    for(l=win->bookmarks->items;l;l=l->next)
    {
        FmBookmarkItem* item = (FmBookmarkItem*)l->data;
        mi = gtk_image_menu_item_new_with_label(item->name);
        gtk_widget_show(mi);
        g_object_set_data_full(G_OBJECT(mi), "path", fm_path_ref(item->path), (GDestroyNotify)fm_path_unref);
        g_signal_connect(mi, "activate", G_CALLBACK(on_bookmark), win);
        gtk_menu_shell_insert(win->bookmarks_menu, mi, i);
        ++i;
    }
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
        if( g_object_get_data(G_OBJECT(item), "path") )
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

static void on_history_item(GtkMenuItem* mi, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    GList* l = g_object_get_data(G_OBJECT(mi), "path");
    fm_tab_page_history(page, l);
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

static void on_show_history_menu(GtkMenuToolButton* btn, FmMainWin* win)
{
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    const GList* l;
    const GList* cur = fm_nav_history_get_cur_link(win->nav_history);

    /* delete old items */
    gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback)gtk_widget_destroy, NULL);

    for(l = fm_nav_history_list(win->nav_history); l; l=l->next)
    {
        const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
        FmPath* path = item->path;
        char* str = fm_path_display_name(path, TRUE);
        GtkWidget* mi;
        if( l == cur )
        {
            mi = gtk_check_menu_item_new_with_label(str);
            gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(mi), TRUE);
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi), TRUE);
        }
        else
            mi = gtk_menu_item_new_with_label(str);
        g_free(str);

        /* FIXME: need to avoid cast from const GList */
        g_object_set_data_full(G_OBJECT(mi), "path", (gpointer)l, NULL);
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
        fm_main_win_add_tab(win, path);
    else
        fm_main_win_chdir(win, path);
}

/* This callback is only connected to side pane of current active tab page. */
static void on_side_pane_mode_changed(FmSidePane* sp, FmMainWin* win)
{

    GList* children;
    GList* child;
    FmSidePaneMode mode;

    if(sp != win->side_pane)
        return;

    children = gtk_container_get_children(GTK_CONTAINER(win->notebook));
    mode = fm_side_pane_get_mode(sp);
    /* set the side pane mode to all other tab pages */
    for(child = children; child; child = child->next)
    {
        FmTabPage* page = FM_TAB_PAGE(child->data);
        if(page != win->current_page)
            fm_side_pane_set_mode(fm_tab_page_get_side_pane(page), mode);
    }
    g_list_free(children);

    /* update menu */
    gtk_radio_action_set_current_value(GTK_RADIO_ACTION(gtk_ui_manager_get_action(win->ui,
                                           "/menubar/ViewMenu/SidePane/Places")),
                                       sp->mode);

    if(mode != app_config->side_pane_mode)
    {
        app_config->side_pane_mode = mode;
        fm_config_emit_changed(FM_CONFIG(app_config), "side_pane_mode");
        pcmanfm_save_config(FALSE);
    }
}

static void fm_main_win_init(FmMainWin *win)
{
    GtkBox *vbox;
    GtkWidget *menubar;
    GtkToolItem *toolitem;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAction* act;
    GtkAccelGroup* accel_grp;
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
    /* FIXME: this is so ugly */
    main_win_toggle_actions[0].is_active = app_config->show_hidden;
    gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions,
                                        G_N_ELEMENTS(main_win_toggle_actions), win);
    gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions,
                                       G_N_ELEMENTS(main_win_mode_actions),
                                       app_config->view_mode,
                                       G_CALLBACK(on_change_mode), win);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions,
                                       G_N_ELEMENTS(main_win_sort_type_actions),
                                       app_config->sort_type,
                                       G_CALLBACK(on_sort_type), win);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions,
                                       G_N_ELEMENTS(main_win_sort_by_actions),
                                       app_config->sort_by,
                                       G_CALLBACK(on_sort_by), win);
    gtk_action_group_add_radio_actions(act_grp, main_win_side_bar_mode_actions,
                                       G_N_ELEMENTS(main_win_side_bar_mode_actions),
                                       app_config->side_pane_mode,
                                       G_CALLBACK(on_side_pane_mode), win);

    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(GTK_WINDOW(win), accel_grp);

    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);

    menubar = gtk_ui_manager_get_widget(ui, "/menubar");
    win->toolbar = GTK_TOOLBAR(gtk_ui_manager_get_widget(ui, "/toolbar"));
    /* FIXME: should make these optional */
    gtk_toolbar_set_icon_size(win->toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(win->toolbar, GTK_TOOLBAR_ICONS);

    /* create 'Next' button manually and add a popup menu to it */
    toolitem = (GtkToolItem*)g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);
    gtk_toolbar_insert(win->toolbar, toolitem, 2);
    gtk_widget_show(GTK_WIDGET(toolitem));
    act = gtk_ui_manager_get_action(ui, "/menubar/GoMenu/Next");
    gtk_activatable_set_related_action(GTK_ACTIVATABLE(toolitem), act);

    /* set up history menu */
    win->history_menu = gtk_menu_new();
    gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolitem), win->history_menu);
    g_signal_connect(toolitem, "show-menu", G_CALLBACK(on_show_history_menu), win);

    win->popup = GTK_MENU(gtk_ui_manager_get_widget(ui, "/popup"));
    gtk_menu_attach_to_widget(win->popup, GTK_WIDGET(win), NULL);

    gtk_box_pack_start( vbox, menubar, FALSE, TRUE, 0 );
    gtk_box_pack_start( vbox, GTK_WIDGET(win->toolbar), FALSE, TRUE, 0 );

    /* load bookmarks menu */
    load_bookmarks(win, ui);

    /* the location bar */
    win->location = fm_path_entry_new();
    g_signal_connect(win->location, "activate", G_CALLBACK(on_location_activate), win);
    if(geteuid() == 0) /* if we're using root, Give the user some warnings */
    {
        GtkWidget* warning = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_widget_set_tooltip_markup(warning, _("You are in super user mode"));

        toolitem = gtk_tool_item_new();
        gtk_container_add( GTK_CONTAINER(toolitem), warning );
        gtk_toolbar_insert(win->toolbar, gtk_separator_tool_item_new(), 0);

        gtk_toolbar_insert(win->toolbar, toolitem, 0);
    }

    toolitem = (GtkToolItem*)gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(toolitem), GTK_WIDGET(win->location));
    gtk_tool_item_set_expand(toolitem, TRUE);
    gtk_toolbar_insert(win->toolbar, toolitem, gtk_toolbar_get_n_items(win->toolbar) - 1);

    /* notebook */
    win->notebook = (GtkNotebook*)gtk_notebook_new();
    gtk_notebook_set_scrollable(win->notebook, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(win->notebook), 0);
    gtk_notebook_set_show_border(win->notebook, FALSE);

    /* We need to use connect_after here.
     * GtkNotebook handles the real page switching stuff in default
     * handler of 'switch-page' signal. The current page is changed to the new one
     * after the signal is emitted. So before the default handler is finished,
     * current page is still the old one. */
    g_signal_connect_after(win->notebook, "switch-page", G_CALLBACK(on_notebook_switch_page), win);
    g_signal_connect(win->notebook, "page-added", G_CALLBACK(on_notebook_page_added), win);
    g_signal_connect(win->notebook, "page-removed", G_CALLBACK(on_notebook_page_removed), win);

    gtk_box_pack_start(vbox, GTK_WIDGET(win->notebook), TRUE, TRUE, 0);

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

    g_object_unref(act_grp);
    win->ui = ui;

    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(vbox));
    gtk_widget_show_all(GTK_WIDGET(vbox));
}


FmMainWin* fm_main_win_new(FmPath* path)
{
    FmMainWin* win = (FmMainWin*)g_object_new(FM_MAIN_WIN_TYPE, NULL);
    /* create new tab */
    fm_main_win_add_tab(win, path);
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
    gtk_window_get_size(GTK_WINDOW(widget), &app_config->win_width, &app_config->win_height);
    (*GTK_WIDGET_CLASS(fm_main_win_parent_class)->unrealize)(widget);
}

static void on_about_response(GtkDialog* dlg, gint response, GtkDialog **dlgptr)
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
        GtkBuilder* builder = gtk_builder_new();
        gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/about.ui", NULL);
        about_dlg = GTK_DIALOG(gtk_builder_get_object(builder, "dlg"));
        g_object_unref(builder);
        g_signal_connect(about_dlg, "response", G_CALLBACK(on_about_response), (gpointer)&about_dlg);
        pcmanfm_ref();
    }
    gtk_window_present(GTK_WINDOW(about_dlg));
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

static void on_open_in_terminal(GtkAction* act, FmMainWin* win)
{
    const FmNavHistoryItem* item = fm_nav_history_get_cur(win->nav_history);
    if(item && item->path)
        pcmanfm_open_folder_in_terminal(GTK_WINDOW(win), item->path);
}

static const char* su_cmd_subst(char opt, gpointer user_data)
{
    return user_data;
}

static FmAppCommandParseOption su_cmd_opts[] =
{
    { 's', su_cmd_subst },
    { 0, NULL }
};

static void on_open_as_root(GtkAction* act, FmMainWin* win)
{
    GAppInfo* app;
    char* cmd;
    if(!app_config->su_cmd)
    {
        fm_show_error(GTK_WINDOW(win), NULL, _("Switch user command is not set."));
        fm_edit_preference(GTK_WINDOW(win), PREF_ADVANCED);
        return;
    }
    /* FIXME: need to rename to pcmanfm when we reach stable release. */
    if(fm_app_command_parse(app_config->su_cmd, su_cmd_opts, &cmd, "pcmanfm %U") == 0)
    {
        /* no %s found so just append to it */
        g_free(cmd);
        cmd = g_strconcat(app_config->su_cmd, " pcmanfm %U", NULL);
    }
    app = g_app_info_create_from_commandline(cmd, NULL, 0, NULL);
    g_free(cmd);
    if(app)
    {
        FmPath* cwd = fm_tab_page_get_cwd(win->current_page);
        GError* err = NULL;
        GdkAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* uri = fm_path_to_uri(cwd);
        GList* uris = g_list_prepend(NULL, uri);
        gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(GTK_WIDGET(win)));
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        if(!g_app_info_launch_uris(app, uris, G_APP_LAUNCH_CONTEXT(ctx), &err))
        {
            fm_show_error(GTK_WINDOW(win), NULL, err->message);
            g_error_free(err);
        }
        g_list_free(uris);
        g_free(uri);
        g_object_unref(ctx);
        g_object_unref(app);
    }
}

static void on_show_hidden(GtkToggleAction* act, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    gboolean active = gtk_toggle_action_get_active(act);
    fm_tab_page_set_show_hidden(page, active);

    if(active != app_config->show_hidden)
    {
        app_config->show_hidden = active;
        pcmanfm_save_config(FALSE);
    }
}

static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int mode = gtk_radio_action_get_current_value(cur);
    fm_folder_view_set_mode( win->folder_view, mode );
}

static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(win->folder_view, -1, val);
    if(val != app_config->sort_by)
    {
        app_config->sort_by = val;
        pcmanfm_save_config(FALSE);
    }
}

static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    guint val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(win->folder_view, val, -1);
    if(val != app_config->sort_type)
    {
        app_config->sort_type = val;
        pcmanfm_save_config(FALSE);
    }
}

static void on_side_pane_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    FmTabPage* cur_page = win->current_page;
    FmSidePane* sp = fm_tab_page_get_side_pane(cur_page);
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


static void on_go(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir(win, fm_path_entry_get_path(win->location));
}

static void on_go_back(GtkAction* act, FmMainWin* win)
{
    fm_tab_page_back(win->current_page);
}

static void on_go_forward(GtkAction* act, FmMainWin* win)
{
    fm_tab_page_forward(win->current_page);
}

static void on_go_up(GtkAction* act, FmMainWin* win)
{
    FmPath* parent = fm_path_get_parent(fm_tab_page_get_cwd(win->current_page));
    if(parent)
        fm_main_win_chdir( win, parent);
}

static void on_go_home(GtkAction* act, FmMainWin* win)
{
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

void fm_main_win_chdir_by_name(FmMainWin* win, const char* path_str)
{
    FmPath* path = fm_path_new_for_str(path_str);
    fm_main_win_chdir(win, path);
    fm_path_unref(path);
}

void fm_main_win_chdir(FmMainWin* win, FmPath* path)
{
    /* NOTE: fm_tab_page_chdir() calls fm_side_pane_chdir(), which can
     * trigger on_side_pane_chdir() callback. So we need to block it here. */
    g_signal_handlers_block_by_func(win->side_pane, on_side_pane_chdir, win);
    fm_tab_page_chdir(win->current_page, path);
    g_signal_handlers_unblock_by_func(win->side_pane, on_side_pane_chdir, win);
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
    gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
    text = fm_tab_page_get_status_text(page, FM_STATUS_TEXT_NORMAL);
    if(text)
        gtk_statusbar_push(win->statusbar, win->statusbar_ctx, text);

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

    gtk_widget_show(gpage);
    g_signal_connect(folder_view, "key-press-event", G_CALLBACK(on_view_key_press_event), win);

    g_signal_connect_swapped(label->close_btn, "clicked", G_CALLBACK(gtk_widget_destroy), page);
    g_signal_connect(label, "button-press-event", G_CALLBACK(on_tab_label_button_pressed), page);

    /* add the tab */
    ret = gtk_notebook_append_page(win->notebook, gpage, GTK_WIDGET(page->tab_label));
    gtk_notebook_set_tab_reorderable(win->notebook, gpage, TRUE);
    gtk_notebook_set_current_page(win->notebook, ret);

    return ret;
}

FmMainWin* fm_main_win_add_win(FmMainWin* win, FmPath* path)
{
    win = fm_main_win_new(path);
    gtk_window_set_default_size(GTK_WINDOW(win),
                                app_config->win_width,
                                app_config->win_height);
    gtk_window_present(GTK_WINDOW(win));
    return win;
}

static void on_cut(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_cut_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_cut_files(GTK_WIDGET(win), files);
            fm_path_list_unref(files);
        }
    }
}

static void on_copy(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_copy_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_copy_files(GTK_WIDGET(win), files);
            fm_path_list_unref(files);
        }
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

static void on_paste(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(win));
    if(GTK_IS_EDITABLE(focus) )
    {
        gtk_editable_paste_clipboard(GTK_EDITABLE(focus));
    }
    else
    {
        FmPath* path = fm_tab_page_get_cwd(win->current_page);
        fm_clipboard_paste_files(GTK_WIDGET(win->folder_view), path);
    }
}

static void on_del(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if(files)
    {
        GdkModifierType state = 0;
        if(!gtk_get_current_event_state (&state))
            state = 0;
        if( state & GDK_SHIFT_MASK ) /* Shift + Delete = delete directly */
            fm_delete_files(GTK_WINDOW(win), files);
        else
            fm_trash_or_delete_files(GTK_WINDOW(win), files);
        fm_path_list_unref(files);
    }
}

static void on_rename(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_dup_selected_file_paths(win->folder_view);
    if( !fm_path_list_is_empty(files) )
    {
        fm_rename_file(GTK_WINDOW(win), fm_path_list_peek_head(files));
        /* FIXME: is it ok to only rename the first selected file here. */
    }
    if(files)
        fm_path_list_unref(files);
}

static void on_select_all(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_all(win->folder_view);
}

static void on_invert_select(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_invert(win->folder_view);
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

static void on_location(GtkAction* act, FmMainWin* win)
{
    gtk_widget_grab_focus(GTK_WIDGET(win->location));
}

static void on_prop(GtkAction* action, FmMainWin* win)
{
    FmFolderView* fv = win->folder_view;
    FmFolder* folder = fm_folder_view_get_folder(fv);
    if(folder && fm_folder_is_valid(folder))
    {
        /* FIXME: should prevent directly accessing data members */
        FmFileInfo* fi = fm_folder_get_info(folder);
        FmFileInfoList* files = fm_file_info_list_new();
        fm_file_info_list_push_tail(files, fi);
        fm_show_file_properties(GTK_WINDOW(win), files);
        fm_file_info_list_unref(files);
    }
}

/* This callback is only connected to folder view of current active tab page. */
static void on_folder_view_clicked(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* fi, FmMainWin* win)
{
    if(fv != win->folder_view)
        return;

    switch(type)
    {
    case FM_FV_ACTIVATED: /* file activated */
        if(fm_file_info_is_dir(fi))
            fm_main_win_chdir( win, fm_file_info_get_path(fi));
        else if(fm_file_info_get_target(fi) && !fm_file_info_is_symlink(fi))
        {
            /* symlinks also has fi->target, but we only handle shortcuts here. */
            FmFileInfo* target_fi;
            FmPath* real_path = fm_path_new_for_str(fm_file_info_get_target(fi));
            /* query the info of target */
            FmFileInfoJob* job = fm_file_info_job_new(NULL, 0);
            fm_file_info_job_add(job, real_path);
            g_signal_connect(job, "error", G_CALLBACK(on_query_target_info_error), win);
            fm_job_run_sync_with_mainloop(FM_JOB(job));
            g_signal_handlers_disconnect_by_func(job, on_query_target_info_error, win);
            target_fi = fm_file_info_list_peek_head(job->file_infos);
            if(target_fi)
                fm_file_info_ref(target_fi);
            g_object_unref(job);
            if(target_fi)
            {
                if(fm_file_info_is_dir(target_fi))
                    fm_main_win_chdir( win, real_path);
                else
                    fm_launch_path_simple(GTK_WINDOW(win), NULL, real_path, open_folder_func, win);
                fm_file_info_unref(target_fi);
            }
            fm_path_unref(real_path);
        }
        else
            fm_launch_file_simple(GTK_WINDOW(win), NULL, fi, open_folder_func, win);
        break;
    case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            FmFileMenu* menu;
            GtkMenu* popup;
            FmFileInfoList* files = fm_folder_view_dup_selected_files(fv);
            menu = fm_file_menu_new_for_files(GTK_WINDOW(win), files, fm_folder_view_get_cwd(fv), TRUE);
            fm_file_menu_set_folder_func(menu, open_folder_func, win);
            fm_file_info_list_unref(files);

            /* merge some specific menu items for folders */
            if(fm_file_menu_is_single_file_type(menu) && fm_file_info_is_dir(fi))
            {
                GtkUIManager* ui = fm_file_menu_get_ui(menu);
                GtkActionGroup* act_grp = fm_file_menu_get_action_group(menu);
                gtk_action_group_set_translation_domain(act_grp, NULL);
                gtk_action_group_add_actions(act_grp, folder_menu_actions, G_N_ELEMENTS(folder_menu_actions), win);
                gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
            }

            popup = fm_file_menu_get_menu(menu);
            gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
        }
        else /* no files are selected. Show context menu of current folder. */
            gtk_menu_popup(win->popup, NULL, NULL, NULL, NULL, 3, gtk_get_current_event_time());
        break;
    case FM_FV_MIDDLE_CLICK:
        if(fm_file_info_is_dir(fi))
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

    switch(type)
    {
    case FM_STATUS_TEXT_NORMAL:
        gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
        if(status_text)
            gtk_statusbar_push(win->statusbar, win->statusbar_ctx, status_text);
        break;
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
    gtk_window_set_title(GTK_WINDOW(win), fm_tab_page_get_title(page));
}

static void on_notebook_switch_page(GtkNotebook* nb, GtkNotebookPage* new_page, guint num, FmMainWin* win)
{
    FmTabPage* page;

    g_return_if_fail(FM_IS_TAB_PAGE(new_page));
    page = (FmTabPage*)new_page;
    /* connect to the new active page */
    win->current_page = page;
    win->folder_view = fm_tab_page_get_folder_view(page);
    win->nav_history = fm_tab_page_get_history(page);
    win->side_pane = fm_tab_page_get_side_pane(page);

    fm_path_entry_set_path(win->location, fm_tab_page_get_cwd(page));
    gtk_window_set_title((GtkWindow*)win, fm_tab_page_get_title(page));

    update_sort_menu(win);
    update_view_menu(win);
    update_statusbar(win);

    /* FIXME: this does not work sometimes due to limitation of GtkNotebook.
     * So weird. After page switching with mouse button, GTK+ always tries
     * to focus the left pane, instead of the folder_view we specified. */
    gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
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
    g_signal_connect(tab_page->folder_view, "sort-changed",
                     G_CALLBACK(on_folder_view_sort_changed), win);
    g_signal_connect(tab_page->folder_view, "clicked",
                     G_CALLBACK(on_folder_view_clicked), win);
    g_signal_connect(tab_page->side_pane, "mode-changed",
                     G_CALLBACK(on_side_pane_mode_changed), win);
    g_signal_connect(tab_page->side_pane, "chdir",
                     G_CALLBACK(on_side_pane_chdir), win);

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

    /* disconnect from previous active page */
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_splitter_pos_changed, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_chdir, win);
    g_signal_handlers_disconnect_by_func(tab_page,
                                         on_tab_page_status_text, win);
    if(folder_view)
    {
        g_signal_handlers_disconnect_by_func(folder_view,
                                             on_view_key_press_event, win);
        g_signal_handlers_disconnect_by_func(tab_page->folder_view,
                                             on_folder_view_sort_changed, win);
        g_signal_handlers_disconnect_by_func(tab_page->folder_view,
                                             on_folder_view_clicked, win);
    }
    g_signal_handlers_disconnect_by_func(tab_page->side_pane,
                                         on_side_pane_mode_changed, win);
    g_signal_handlers_disconnect_by_func(tab_page->side_pane,
                                         on_side_pane_chdir, win);

    if(tab_page == win->current_page)
    {
        win->current_page = NULL;
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

static void on_create_new(GtkAction* action, FmMainWin* win)
{
    const char* name = gtk_action_get_name(action);

    if( strcmp(name, "NewFolder") == 0 )
        name = TEMPL_NAME_FOLDER;
    else if( strcmp(name, "NewBlank") == 0 )
        name = TEMPL_NAME_BLANK;
    else if( strcmp(name, "NewShortcut") == 0 )
        name = TEMPL_NAME_SHORTCUT;
    pcmanfm_create_new(GTK_WINDOW(win), fm_tab_page_get_cwd(win->current_page), name);
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
        if(evt->keyval == GDK_Tab
         || evt->keyval == GDK_ISO_Left_Tab
         || evt->keyval == GDK_Page_Down) /* Ctrl + Tab or PageDown, next tab */
        {
            switch_to_next_tab(win);
            return TRUE;
        }
        else if(evt->keyval == GDK_Page_Up)
        {
            switch_to_prev_tab(win);
            return TRUE;
        }
    }
    else if(modifier == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) /* Ctrl + Shift */
    {
        if(evt->keyval == GDK_Tab
         || evt->keyval == GDK_ISO_Left_Tab) /* Ctrl + Shift + Tab or PageUp, previous tab */
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
    else if(evt->keyval == GDK_Escape)
    {
        if (gtk_widget_is_focus(GTK_WIDGET(win->location)))
        {
            gtk_widget_grab_focus(GTK_WIDGET(win->folder_view));
            fm_path_entry_set_path(win->location,
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

static void on_reload(GtkAction* act, FmMainWin* win)
{
    FmTabPage* page = win->current_page;
    fm_tab_page_reload(page);
}

static void on_show_side_pane(GtkToggleAction* act, FmMainWin* win)
{
    /* TODO: hide the side pane if the user wants to. */
}
