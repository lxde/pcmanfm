//      fm-tab-page.c
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
//      Copyright 2012-2015 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
//      Copyright 2020,2024 Ingo Brückl
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
#include <unistd.h>

/* Additional entries for FmFileMenu popup */
/* it is also used for FmSidePane context menu popup */
static const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<menuitem action='OpenInOtherPane'/>"
    "<menuitem action='OpenInNewPane'/>"
    "<menuitem action='Term'/>"
    "<menuitem action='AsRoot'/>"
    /* "<menuitem action='Search'/>" */
  "</placeholder>"
"</popup>";

static void on_open_in_new_tab(GtkAction* act, FmMainWin* win);
static void on_open_in_new_win(GtkAction* act, FmMainWin* win);
static void on_open_folder_in_terminal(GtkAction* act, FmMainWin* win);
static void on_open_folder_as_root(GtkAction* act, FmMainWin* win);
static void on_open_in_other_pane(GtkAction* act, FmMainWin* win);
static void on_open_in_new_pane(GtkAction* act, FmMainWin* win);

/* Action entries for popup menu entries above */
static GtkActionEntry folder_menu_actions[]=
{
    {"NewTab", "tab-new", N_("Open in New T_ab"), NULL, NULL, G_CALLBACK(on_open_in_new_tab)},
    {"NewWin", GTK_STOCK_NEW, N_("Open in New Win_dow"), NULL, NULL, G_CALLBACK(on_open_in_new_win)},
    {"OpenInOtherPane", NULL, N_("Open in Other _Pane"), NULL, NULL, G_CALLBACK(on_open_in_other_pane)},
    {"OpenInNewPane", NULL, N_("Open in New Pan_e"), NULL, NULL, G_CALLBACK(on_open_in_new_pane)},
    {"Search", GTK_STOCK_FIND, NULL, NULL, NULL, NULL},
    {"Term", "utilities-terminal", N_("Open in Termina_l"), NULL, NULL, G_CALLBACK(on_open_folder_in_terminal)},
    {"AsRoot", GTK_STOCK_DIALOG_AUTHENTICATION, N_("Open as _Root"), NULL, NULL, G_CALLBACK(on_open_folder_as_root)},
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

/* One pane is either the page itself (primary pane) or a child FmTabPage
   embedded into the page's views box (additional pane). */
struct _FmTabPagePane
{
    FmTabPage *host;
    FmTabPage *sub; /* NULL for the primary pane */
    GtkWidget *box;
    GtkWidget *loc_box;
    GtkWidget *entry;
    GtkWidget *bar;
    GtkWidget *close_btn;
};

static void on_pane_close_clicked(GtkButton *btn, FmTabPagePane *pane);
static void _update_pane_close_buttons(FmTabPage *page);

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
    /* page might be just loaded so stop updating it in any case */
    if(page->update_scroll_id)
    {
        g_source_remove(page->update_scroll_id);
        page->update_scroll_id = 0;
    }
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

/* ---- multi-pane support ---- */

static FmFolderView *pane_get_view(FmTabPagePane *pane)
{
    FmTabPage *page = pane->sub ? pane->sub : pane->host;

    return page->folder_view;
}

/* Keep the notebook tab title in sync with the active pane of the page. A
   page embedded as an extra pane has no visible tab of its own, so only the
   host page is updated here. */
static void _update_host_label(FmTabPage *page)
{
    FmTabPage *active;
    FmPath *path;
    char *disp_name, *tip;

    if (page->is_subpage || !page->panes || page->panes->len == 0)
        return;
    if (page->active_pane < page->panes->len)
    {
        FmTabPagePane *pane = g_ptr_array_index(page->panes, page->active_pane);
        active = pane->sub ? pane->sub : page;
    }
    else
        active = page;
    if (!active->folder)
        return;
    path = fm_folder_get_path(active->folder);
    disp_name = fm_path_display_basename(path);
#if FM_CHECK_VERSION(1, 0, 2)
    if (active->filter_pattern && active->filter_pattern[0])
    {
        char *text = g_strdup_printf("%s [%s]", disp_name,
                                     active->filter_pattern);
        g_free(disp_name);
        disp_name = text;
    }
#endif
    fm_tab_label_set_text(page->tab_label, disp_name);
    g_free(disp_name);
    tip = fm_path_display_name(path, FALSE);
    fm_tab_label_set_tooltip_text(FM_TAB_LABEL(page->tab_label), tip);
    g_free(tip);
}

static void _update_primary_location(FmTabPage *page, FmPath *path)
{
    FmTabPagePane *pane;

    if (!page->panes || page->panes->len == 0)
        return;
    pane = g_ptr_array_index(page->panes, 0);
    if (!path)
        path = pane_get_view(pane) ? fm_folder_view_get_cwd(pane_get_view(pane)) : NULL;
    if (!path)
        return;
    fm_path_entry_set_path(FM_PATH_ENTRY(pane->entry), path);
    fm_path_bar_set_path(FM_PATH_BAR(pane->bar), path);
}

static void _set_pane_path_mode(FmTabPagePane *pane, gboolean buttons)
{
    gtk_widget_set_visible(pane->entry, !buttons);
    gtk_widget_set_visible(pane->bar, buttons);
}

static void _set_primary_path_mode(FmTabPage *page, gboolean buttons)
{
    FmTabPagePane *pane;

    if (!page->panes || page->panes->len == 0)
        return;
    pane = g_ptr_array_index(page->panes, 0);
    _set_pane_path_mode(pane, buttons);
}

static void _update_primary_bar_visibility(FmTabPage *page)
{
    FmTabPagePane *pane;

    if (!page->panes || page->panes->len == 0)
        return;
    pane = g_ptr_array_index(page->panes, 0);
    gtk_widget_set_visible(pane->loc_box, page->panes->len > 1);
}

static void on_pane_entry_activate(GtkEntry *entry, FmTabPagePane *pane)
{
    FmTabPage *page = pane->sub ? pane->sub : pane->host;
    FmPath *path = fm_path_entry_get_path(FM_PATH_ENTRY(entry));

    if (!path)
        return;
    fm_path_ref(path);
    fm_tab_page_chdir(page, path);
    fm_path_unref(path);
    /* recover from Ctrl+L mode change */
    _set_primary_path_mode(page, app_config->pathbar_mode_buttons);
    gtk_widget_grab_focus(GTK_WIDGET(pane_get_view(pane)));
}

static void on_pane_bar_chdir(FmPathBar *bar, FmPath *path, FmTabPagePane *pane)
{
    FmTabPage *page = pane->sub ? pane->sub : pane->host;

    fm_tab_page_chdir(page, path);
    gtk_widget_grab_focus(GTK_WIDGET(pane_get_view(pane)));
}

/* Close the pane this button belongs to and keep the window's pane count in
   sync with the result. */
static void on_pane_close_clicked(GtkButton *btn, FmTabPagePane *pane)
{
    FmTabPage *host = pane->host;
    FmMainWin *win;
    guint i;

    if (!host->panes)
        return;
    for (i = 0; i < host->panes->len; i++)
        if (g_ptr_array_index(host->panes, i) == pane)
            break;
    if (i >= host->panes->len)
        return;
    if (!fm_tab_page_close_pane(host, i))
        return;
    win = GET_MAIN_WIN(host);
    if (win && IS_FM_MAIN_WIN(win))
        fm_main_win_set_pane_count(win, fm_tab_page_get_n_panes(host));
}

static void on_subpage_chdir(FmTabPage *sub, FmPath *path, FmTabPagePane *pane)
{
    if (pane->host->active_pane < pane->host->panes->len
        && g_ptr_array_index(pane->host->panes, pane->host->active_pane) == pane)
    {
        _update_host_label(pane->host);
        g_signal_emit(pane->host, signals[CHDIR], 0, path);
    }
}

static void on_subpage_status(FmTabPage *sub, guint type, const char *text,
                              FmTabPagePane *pane)
{
    if (pane->host->active_pane < pane->host->panes->len
        && g_ptr_array_index(pane->host->panes, pane->host->active_pane) == pane)
        g_signal_emit(pane->host, signals[STATUS], 0, type, text);
}

static void on_subpage_got_focus(FmTabPage *sub, FmTabPagePane *pane)
{
    guint i;

    for (i = 0; i < pane->host->panes->len; i++)
        if (g_ptr_array_index(pane->host->panes, i) == pane)
        {
            fm_tab_page_set_active_pane(pane->host, i);
            return;
        }
}

static FmTabPagePane *_pane_new_primary(FmTabPage *page)
{
    FmTabPagePane *pane = g_slice_new0(FmTabPagePane);

    pane->host = page;
    pane->sub = NULL;
    pane->box = gtk_vbox_new(FALSE, 0);
    pane->loc_box = gtk_hbox_new(FALSE, 0);
    pane->entry = GTK_WIDGET(fm_path_entry_new());
    pane->bar = GTK_WIDGET(fm_path_bar_new());
#if GTK_CHECK_VERSION(3, 0, 0)
    pane->close_btn = gtk_button_new_from_icon_name("window-close",
                                                    GTK_ICON_SIZE_MENU);
#else
    pane->close_btn = gtk_button_new_from_icon_name("gtk-close",
                                                    GTK_ICON_SIZE_MENU);
#endif
    gtk_button_set_relief(GTK_BUTTON(pane->close_btn), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(pane->close_btn), FALSE);
    gtk_widget_set_tooltip_text(pane->close_btn, _("Close this pane"));
    gtk_box_pack_start(GTK_BOX(pane->loc_box), pane->entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pane->loc_box), pane->bar, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(pane->loc_box), pane->close_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pane->box), pane->loc_box, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pane->box), GTK_WIDGET(page->folder_view),
                       TRUE, TRUE, 0);
    g_signal_connect(pane->entry, "activate",
                     G_CALLBACK(on_pane_entry_activate), pane);
    g_signal_connect(pane->bar, "chdir",
                     G_CALLBACK(on_pane_bar_chdir), pane);
    g_signal_connect(pane->close_btn, "clicked",
                     G_CALLBACK(on_pane_close_clicked), pane);
    g_ptr_array_add(page->panes, pane);
    gtk_box_pack_start(page->views, pane->box, TRUE, TRUE, 0);
    /* the bar of the primary pane is shown only in multi-pane mode */
    gtk_widget_show_all(pane->box);
    /* show_all() reveals both widgets, apply the configured mode again */
    _set_pane_path_mode(pane, app_config->pathbar_mode_buttons);
    _update_primary_bar_visibility(page);
    return pane;
}

static void _pane_free(FmTabPagePane *pane)
{
    if (pane->sub)
    {
        FmTabPage *sub = pane->sub;
        FmTabPagePane *sub_pane = (sub->panes && sub->panes->len > 0)
                                  ? g_ptr_array_index(sub->panes, 0) : NULL;

        /* the subpage signals carry this pane as user data; disconnect them
           before the pane is freed, even if the widget is not finalized yet */
        g_signal_handlers_disconnect_by_func(sub, on_subpage_chdir, pane);
        g_signal_handlers_disconnect_by_func(sub, on_subpage_status, pane);
        g_signal_handlers_disconnect_by_func(sub, on_subpage_got_focus, pane);
        if (sub_pane && sub_pane->close_btn)
            g_signal_handlers_disconnect_by_func(sub_pane->close_btn,
                                                 on_pane_close_clicked, pane);
        gtk_widget_destroy(GTK_WIDGET(sub));
        pane->sub = NULL;
    }
    g_slice_free(FmTabPagePane, pane);
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
    if (page->panes)
    {
        guint i;

        for (i = page->panes->len; i > 1; i--)
        {
            FmTabPagePane *pane = g_ptr_array_index(page->panes, i - 1);

            g_ptr_array_remove_index(page->panes, i - 1);
            _pane_free(pane);
        }
        if (page->panes->len > 0)
            g_slice_free(FmTabPagePane, g_ptr_array_index(page->panes, 0));
        g_ptr_array_free(page->panes, TRUE);
        page->panes = NULL;
    }
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
        if (page->side_pane)
            g_signal_handlers_disconnect_by_func(app_config, on_home_path_changed,
                                                 page->side_pane);
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
    if (page->side_pane)
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
                /* Note to translators: this is the information (name, size, type)
                   about the selected file in the status bar */
                g_string_printf(str, _("\"%s\" (%s) %s"),
                            fm_file_info_get_disp_name(fi),
                            size_str ? size_str : "",
                            fm_file_info_get_desc(fi));
            }
            else
            {
                /* Note to translators: this is the information (name, type)
                   about the selected file in the status bar */
                g_string_printf(str, _("\"%s\" %s"),
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
    fm_tab_page_set_active_pane(page, 0);
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
    GtkScrolledWindow* scroll;
#if !FM_CHECK_VERSION(1, 0, 2)
    const FmNavHistoryItem* item;
#endif

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    scroll = GTK_SCROLLED_WINDOW(page->folder_view);
#if !FM_CHECK_VERSION(1, 0, 2)
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
    if (page->update_scroll_id)
        g_source_remove(page->update_scroll_id);
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

static void on_open_folder_as_root(GtkAction* act, FmMainWin* win)
{
    GObject* act_grp;
    FmFileInfoList* files;
    GList* l;
    GList* paths = NULL;

    g_object_get(act, "action-group", &act_grp, NULL);
    files = g_object_get_qdata(act_grp, popup_qdata);
    g_object_unref(act_grp);

    if (files)
    {
        for (l = fm_file_info_list_peek_head_link(files); l; l = l->next)
        {
            FmFileInfo* fi = (FmFileInfo*)l->data;
            if (fm_file_info_is_dir(fi))
                paths = g_list_prepend(paths, fm_file_info_get_path(fi));
        }
    }
    if (!paths && win->current_page)
    {
        FmPath* cwd = fm_tab_page_get_cwd(win->current_page);
        if (cwd)
            paths = g_list_prepend(paths, cwd);
    }
    if (paths)
    {
        pcmanfm_open_folders_as_root(GTK_WINDOW(win), paths);
        g_list_free(paths);
    }
}

static FmPath *popup_get_path(GtkAction *act, FmMainWin *win)
{
    GObject *act_grp;
    FmFileInfoList *files;
    FmFileInfo *fi;
    FmPath *path = NULL;

    g_object_get(act, "action-group", &act_grp, NULL);
    files = g_object_get_qdata(act_grp, popup_qdata);
    g_object_unref(act_grp);
    if (files)
    {
        fi = fm_file_info_list_peek_head(files);
        if (fi && fm_file_info_is_dir(fi))
            path = fm_file_info_get_path(fi);
    }
    if (!path && win->current_page)
        path = fm_tab_page_get_cwd(win->current_page);
    return path;
}

static void on_open_in_other_pane(GtkAction* act, FmMainWin* win)
{
    FmTabPage *page = win->current_page;
    guint n, active, target;
    FmPath *path = popup_get_path(act, win);

    if (!page || !path)
        return;
    n = fm_tab_page_get_n_panes(page);
    active = fm_tab_page_get_active_pane(page);
    if (n < 2)
        target = 1;
    else
        target = (active + 1) % n;
    fm_path_ref(path);
    fm_main_win_open_in_pane(win, path, target);
    fm_path_unref(path);
}

static void on_open_in_new_pane(GtkAction* act, FmMainWin* win)
{
    FmTabPage *page = win->current_page;
    guint n, target;
    FmPath *path = popup_get_path(act, win);

    if (!page || !path)
        return;
    n = fm_tab_page_get_n_panes(page);
    target = (n < FM_MAX_PANES) ? n : FM_MAX_PANES - 1;
    fm_path_ref(path);
    fm_main_win_open_in_pane(win, path, target);
    fm_path_unref(path);
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
        else if (!pcmanfm_can_open_path_in_terminal(fm_file_info_get_path(l->data)))
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
    if (!all_native || geteuid() == 0)
        gtk_action_set_visible(gtk_action_group_get_action(act_grp, "AsRoot"), FALSE);
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
    if (!pcmanfm_can_open_path_in_terminal(fm_file_info_get_path(file)))
        gtk_action_set_visible(gtk_action_group_get_action(act_grp, "Term"), FALSE);
    if (!pcmanfm_can_open_path_in_terminal(fm_file_info_get_path(file)) || geteuid() == 0)
        gtk_action_set_visible(gtk_action_group_get_action(act_grp, "AsRoot"), FALSE);
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

static void on_side_pane_destroy(GtkWidget *widget, FmTabPage *page)
{
    if (page->side_pane == FM_SIDE_PANE(widget))
        page->side_pane = NULL;
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
    g_signal_connect(page->side_pane, "destroy",
                     G_CALLBACK(on_side_pane_destroy), page);
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
    page->panes = g_ptr_array_new();
    page->active_pane = 0;
    gtk_paned_add2(paned, GTK_WIDGET(page->views));
    _pane_new_primary(page);
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
    /* drop columns of the previous folder, they are set again below if needed */
    g_strfreev(page->columns);
    page->columns = NULL;
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
    g_strfreev(columns);
#else
    /* since 1.0.2 sorting should be applied on model instead */
    fm_folder_view_sort(page->folder_view, page->sort_type, page->sort_by);
#endif

    fm_side_pane_chdir(page->side_pane, path);

    /* update the location bar of the primary pane */
    _update_primary_location(page, path);
    /* if this is the host page, its tab shows the active pane */
    _update_host_label(page);

    /* tell the world that our current working directory is changed */
    g_signal_emit(page, signals[CHDIR], 0, path);
}

void fm_tab_page_chdir(FmTabPage* page, FmPath* path)
{
    FmPath* cwd = page->folder ? fm_folder_get_path(page->folder) : NULL;
    int scroll_pos;
    if(cwd && path && fm_path_equal(cwd, path))
        return;
    scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view)));
    fm_nav_history_chdir(page->nav_history, path, scroll_pos);
    fm_tab_page_chdir_without_history(page, path);
}

void fm_tab_page_set_show_hidden(FmTabPage* page, gboolean show_hidden)
{
    page->show_hidden = show_hidden;
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
    FmTabPage *active = fm_tab_page_get_active_subpage(page);

    return active->folder ? fm_folder_get_path(active->folder) : NULL;
}

FmSidePane* fm_tab_page_get_side_pane(FmTabPage* page)
{
    return page->side_pane;
}

FmFolderView* fm_tab_page_get_folder_view(FmTabPage* page)
{
    return fm_tab_page_get_active_subpage(page)->folder_view;
}

FmFolderView* fm_tab_page_get_primary_view(FmTabPage* page)
{
    return page->folder_view;
}

FmFolder* fm_tab_page_get_folder(FmTabPage* page)
{
    return fm_folder_view_get_folder(fm_tab_page_get_active_subpage(page)->folder_view);
}

FmNavHistory* fm_tab_page_get_history(FmTabPage* page)
{
    return fm_tab_page_get_active_subpage(page)->nav_history;
}

void fm_tab_page_forward(FmTabPage* page)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);

    if (active != page)
    {
        fm_tab_page_forward(active);
        return;
    }
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
    FmTabPage *active = fm_tab_page_get_active_subpage(page);

    if (active != page)
    {
        fm_tab_page_back(active);
        return;
    }
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
    FmTabPage *active = fm_tab_page_get_active_subpage(page);
    GtkAdjustment* vadjustment;

    if (active != page)
    {
        fm_tab_page_history(active, history_item);
        return;
    }
    vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
    int scroll_pos = gtk_adjustment_get_value(vadjustment);
    FmPath *path = fm_nav_history_go_to(page->nav_history, history_item, scroll_pos);
    fm_tab_page_chdir_without_history(page, path);
}
#else
void fm_tab_page_history(FmTabPage* page, GList* history_item_link)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);
    const FmNavHistoryItem* item;
    GtkAdjustment* vadjustment;

    if (active != page)
    {
        fm_tab_page_history(active, history_item_link);
        return;
    }
    item = (FmNavHistoryItem*)history_item_link->data;
    vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page->folder_view));
    int scroll_pos = gtk_adjustment_get_value(vadjustment);
    fm_nav_history_jump(page->nav_history, history_item_link, scroll_pos);
    item = fm_nav_history_get_cur(page->nav_history);
    fm_tab_page_chdir_without_history(page, item->path);
}
#endif

const char* fm_tab_page_get_title(FmTabPage* page)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);
    FmTabLabel* label = active->tab_label;

    return gtk_label_get_text(label->label);
}

const char* fm_tab_page_get_status_text(FmTabPage* page, FmStatusTextType type)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);

    return (type < FM_STATUS_TEXT_NUM) ? active->status_text[type] : NULL;
}

void fm_tab_page_reload(FmTabPage* page)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);
    FmFolder* folder;

    if (active != page)
    {
        fm_tab_page_reload(active);
        return;
    }
    folder = fm_folder_view_get_folder(page->folder_view);

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

guint fm_tab_page_get_n_panes(FmTabPage *page)
{
    return page->panes ? page->panes->len : 0;
}

guint fm_tab_page_get_active_pane(FmTabPage *page)
{
    return page->active_pane;
}

FmTabPage *fm_tab_page_get_nth_subpage(FmTabPage *page, guint index)
{
    FmTabPagePane *pane;

    if (!page->panes)
        /* the page can be queried while being destroyed, be safe */
        return index == 0 ? page : NULL;
    if (index >= page->panes->len)
        return NULL;
    pane = g_ptr_array_index(page->panes, index);
    return pane->sub ? pane->sub : page;
}

FmTabPage *fm_tab_page_get_active_subpage(FmTabPage *page)
{
    FmTabPage *sub = fm_tab_page_get_nth_subpage(page, page->active_pane);

    return sub ? sub : page;
}

FmFolderView *fm_tab_page_get_nth_view(FmTabPage *page, guint index)
{
    FmTabPage *sub = fm_tab_page_get_nth_subpage(page, index);

    return sub ? sub->folder_view : NULL;
}

FmPath *fm_tab_page_get_nth_cwd(FmTabPage *page, guint index)
{
    FmTabPage *sub = fm_tab_page_get_nth_subpage(page, index);

    return (sub && sub->folder) ? fm_folder_get_path(sub->folder) : NULL;
}

void fm_tab_page_chdir_nth(FmTabPage *page, guint index, FmPath *path)
{
    FmTabPage *sub = fm_tab_page_get_nth_subpage(page, index);

    if (sub)
        fm_tab_page_chdir(sub, path);
}

void fm_tab_page_focus_nth(FmTabPage *page, guint index)
{
    FmTabPage *sub = fm_tab_page_get_nth_subpage(page, index);

    if (!sub)
        return;
    fm_tab_page_set_active_pane(page, index);
    if (sub->folder_view)
        gtk_widget_grab_focus(GTK_WIDGET(sub->folder_view));
}

gint fm_tab_page_find_view(FmTabPage *page, FmFolderView *view)
{
    guint i;

    if (!page->panes)
        return -1;
    for (i = 0; i < page->panes->len; i++)
    {
        FmTabPagePane *pane = g_ptr_array_index(page->panes, i);

        if (pane_get_view(pane) == view)
            return (gint)i;
    }
    return -1;
}

static void _update_pane_close_buttons(FmTabPage *page)
{
    guint i;

    if (!page->panes)
        return;
    for (i = 0; i < page->panes->len; i++)
    {
        FmTabPagePane *pane = g_ptr_array_index(page->panes, i);
        FmTabPagePane *visible_pane = pane;

        /* an extra pane is a child page, its widgets live on that page */
        if (pane->sub && pane->sub->panes && pane->sub->panes->len > 0)
            visible_pane = g_ptr_array_index(pane->sub->panes, 0);
        if (visible_pane->close_btn)
            gtk_widget_set_visible(visible_pane->close_btn, TRUE);
    }
}

static void _update_pane_states(FmTabPage *page)
{
    guint i;

    if (!page->panes)
        return;
    for (i = 0; i < page->panes->len; i++)
    {
        FmTabPagePane *pane = g_ptr_array_index(page->panes, i);

        gtk_widget_set_state(GTK_WIDGET(pane_get_view(pane)),
                             i == page->active_pane
                                 ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL);
    }
}

void fm_tab_page_set_active_pane(FmTabPage *page, guint index)
{
    if (!page->panes || index >= page->panes->len)
        return;
    page->active_pane = index;
    _update_pane_states(page);
    _update_host_label(page);
    /* always notify: focus can move into an already active pane and the
       window still has to switch its notion of the active folder view */
    g_signal_emit(page, signals[GOT_FOCUS], 0);
}

/* Remove one pane and keep the remaining panes consistent. Closing the
   primary pane promotes the next pane into the host page, because the host
   page itself cannot be removed from the notebook tab. */
gboolean fm_tab_page_close_pane(FmTabPage *page, guint index)
{
    FmTabPagePane *pane;

    if (!page->panes || page->panes->len <= 1 || index >= page->panes->len)
        return FALSE;

    if (index == 0)
    {
        FmTabPagePane *next = g_ptr_array_index(page->panes, 1);
        FmTabPage *sub = next->sub;
        FmPath *path = NULL;
        FmStandardViewMode view_mode;
        gboolean show_hidden;
#if FM_CHECK_VERSION(1, 0, 2)
        FmSortMode sort_type;
        FmFolderModelCol sort_by;
        FmFolderModel *model;
        char *filter_pattern = NULL;
#else
        GtkSortType sort_type;
        int sort_by;
#endif

        if (!sub)
            return FALSE;

        /* remember the state of the pane which is going to become primary */
        if (sub->folder)
        {
            path = fm_folder_get_path(sub->folder);
            fm_path_ref(path);
        }
        view_mode = fm_standard_view_get_mode(FM_STANDARD_VIEW(sub->folder_view));
        show_hidden = sub->show_hidden;
        sort_type = sub->sort_type;
        sort_by = sub->sort_by;
#if FM_CHECK_VERSION(1, 0, 2)
        filter_pattern = g_strdup(sub->filter_pattern);
#endif

        /* switch the window away from the view which is about to be
           destroyed before the pane disappears */
        if (page->active_pane <= 1)
            fm_tab_page_set_active_pane(page, 0);
        g_ptr_array_remove_index(page->panes, 1);
        _pane_free(next);
        if (page->active_pane > 1)
            page->active_pane--;

        if (path)
        {
            FmMainWin *win = GET_MAIN_WIN(page);
            GtkWidget *path_bar = (win && IS_FM_MAIN_WIN(win))
                                  ? GTK_WIDGET(win->path_bar) : NULL;
            guint sid = path_bar ? g_signal_lookup("chdir",
                                                   G_OBJECT_TYPE(path_bar)) : 0;

            /* changing the host folder also updates the window path bar;
               its "chdir" signal must not navigate the active pane back to
               the folder we are promoting into the host */
            if (sid)
                g_signal_handlers_block_matched(path_bar, G_SIGNAL_MATCH_ID,
                                                sid, 0, NULL, NULL, NULL);
            fm_tab_page_chdir_without_history(page, path);
            if (sid)
                g_signal_handlers_unblock_matched(path_bar, G_SIGNAL_MATCH_ID,
                                                  sid, 0, NULL, NULL, NULL);
            fm_path_unref(path);
        }

        /* re-apply the promoted pane's own view state, chdir() would have
           taken the defaults from the folder config instead */
        page->sort_type = sort_type;
        page->sort_by = sort_by;
        page->view_mode = view_mode;
        fm_standard_view_set_mode(FM_STANDARD_VIEW(page->folder_view), view_mode);
#if FM_CHECK_VERSION(1, 0, 2)
        model = fm_folder_view_get_model(page->folder_view);
        if (model)
            fm_folder_model_set_sort(model, sort_by, sort_type);
        fm_tab_page_set_filter_pattern(page, filter_pattern);
        g_free(filter_pattern);
#else
        fm_folder_view_sort(page->folder_view, sort_type, sort_by);
#endif
        fm_tab_page_set_show_hidden(page, show_hidden);

        _update_primary_bar_visibility(page);
        _update_pane_states(page);
        _update_pane_close_buttons(page);
        _update_host_label(page);
        g_signal_emit(page, signals[GOT_FOCUS], 0);
        return TRUE;
    }

    /* switch away before the pane's folder view is destroyed */
    if (page->active_pane == index)
        fm_tab_page_set_active_pane(page, index - 1);
    pane = g_ptr_array_index(page->panes, index);
    g_ptr_array_remove_index(page->panes, index);
    _pane_free(pane);
    if (page->active_pane > index)
        page->active_pane--;
    _update_primary_bar_visibility(page);
    _update_pane_states(page);
    _update_pane_close_buttons(page);
    _update_host_label(page);
    g_signal_emit(page, signals[GOT_FOCUS], 0);
    return TRUE;
}

void fm_tab_page_set_n_panes(FmTabPage *page, guint n_panes)
{
    if (!page->panes)
        return;
    if (n_panes < 1)
        n_panes = 1;
    else if (n_panes > FM_MAX_PANES)
        n_panes = FM_MAX_PANES;

    while (page->panes->len < n_panes)
    {
        FmTabPagePane *pane = g_slice_new0(FmTabPagePane);
        FmTabPagePane *sub_pane;
        FmTabPage *sub;
        FmPath *path = fm_tab_page_get_cwd(page);

        if (!path)
            path = fm_path_get_home();
        sub = fm_tab_page_new(path);
        sub->is_subpage = TRUE;
        sub_pane = g_ptr_array_index(sub->panes, 0);
        gtk_widget_set_visible(sub_pane->loc_box, TRUE);

        pane->host = page;
        pane->sub = sub;
        pane->box = GTK_WIDGET(sub);
        g_ptr_array_add(page->panes, pane);
        g_signal_connect(sub, "chdir", G_CALLBACK(on_subpage_chdir), pane);
        g_signal_connect(sub, "status", G_CALLBACK(on_subpage_status), pane);
        g_signal_connect(sub, "got-focus",
                         G_CALLBACK(on_subpage_got_focus), pane);
        /* the button belongs to the outer pane, not to the embedded page */
        g_signal_handlers_disconnect_by_func(sub_pane->close_btn,
                                             on_pane_close_clicked, sub_pane);
        g_signal_connect(sub_pane->close_btn, "clicked",
                         G_CALLBACK(on_pane_close_clicked), pane);
        gtk_box_pack_start(page->views, pane->box, TRUE, TRUE, 0);
        /* show all children first, then hide what is not needed */
        gtk_widget_show_all(pane->box);
        /* an additional pane has no side pane of its own */
        gtk_widget_hide(GTK_WIDGET(sub->side_pane));
        gtk_widget_set_visible(sub_pane->loc_box, TRUE);
        _set_primary_path_mode(sub, app_config->pathbar_mode_buttons);
    }

    /* do not keep an active pane that is going to be destroyed; silently
       move to the last pane that survives */
    if (page->active_pane >= n_panes)
        page->active_pane = n_panes - 1;

    while (page->panes->len > n_panes)
    {
        FmTabPagePane *pane;

        pane = g_ptr_array_index(page->panes, page->panes->len - 1);
        g_ptr_array_remove_index(page->panes, page->panes->len - 1);
        _pane_free(pane);
    }

    _update_primary_bar_visibility(page);
    _update_pane_states(page);
    _update_pane_close_buttons(page);
    _update_host_label(page);
}

void fm_tab_page_set_path_mode(FmTabPage *page, gboolean buttons)
{
    guint i;

    if (!page->panes)
        return;
    for (i = 0; i < page->panes->len; i++)
    {
        FmTabPagePane *pane = g_ptr_array_index(page->panes, i);

        if (pane->sub)
            fm_tab_page_set_path_mode(pane->sub, buttons);
        else
        {
            gtk_widget_set_visible(pane->entry, !buttons);
            gtk_widget_set_visible(pane->bar, buttons);
        }
    }
}

gboolean fm_tab_page_focus_location(FmTabPage *page)
{
    FmTabPage *active = fm_tab_page_get_active_subpage(page);
    FmTabPagePane *pane;

    if (active != page)
        return fm_tab_page_focus_location(active);
    if (!page->panes || page->panes->len == 0)
        return FALSE;
    pane = g_ptr_array_index(page->panes, 0);
    gtk_widget_show(pane->entry);
    gtk_widget_hide(pane->bar);
    gtk_widget_grab_focus(pane->entry);
    return TRUE;
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
