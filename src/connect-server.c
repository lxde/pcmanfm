/*
 *      connect-server.c
 *
 *      This file is a part of the PCManFM project.
 *
 *      Copyright 2013-2014 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#include "connect-server.h"

#include <string.h>

#include "main-win.h"

typedef struct
{
    GtkWindow *dlg;
    GtkWidget *ok;
    GtkComboBox *server_type;
    GtkEntry *server_host;
    GtkSpinButton *server_port;
    GtkEntry *server_path;
    GtkToggleButton *user_anonymous;
    GtkToggleButton *user_user;
    GtkEntry *login_entry;
} ConnectDlg;

static void on_response(GtkDialog *dialog, gint resp, ConnectDlg *dlg)
{
    GtkWindow *parent = gtk_window_get_transient_for(dlg->dlg);
    GString *str;
    GtkTreeIter iter;
    char *scheme = NULL;
    const char *text;
    FmPath *path;
    int def_port, used_port;

    if (resp == GTK_RESPONSE_OK && gtk_combo_box_get_active_iter(dlg->server_type, &iter))
        gtk_tree_model_get(gtk_combo_box_get_model(dlg->server_type), &iter, 1, &scheme, -1);
    if (scheme)
    {
        /* make an URI from the data */
        str = g_string_new(scheme);
        g_free(scheme);
        g_string_append(str, "://");
        if (gtk_toggle_button_get_active(dlg->user_user))
            g_string_append_printf(str, "%s@", gtk_entry_get_text(dlg->login_entry));
        g_string_append(str, gtk_entry_get_text(dlg->server_host));
        if (strcmp(scheme, "sftp") == 0)
            def_port = 22;
        else if (strcmp(scheme, "ftp") == 0)
            def_port = 21;
        else if (strcmp(scheme, "dav") == 0)
            def_port = 80;
        else
            def_port = -1;
        used_port = (int)gtk_spin_button_get_value(dlg->server_port);
        if (def_port != used_port)
            g_string_append_printf(str, ":%d", used_port);
        text = gtk_entry_get_text(dlg->server_path);
        if (text[0] != '/')
            g_string_append_c(str, '/');
        g_string_append(str, text);
        path = fm_path_new_for_uri(str->str);
        /* g_debug("created URI: %s",str->str); */
        g_string_free(str, TRUE);
        /* create new tab for the URI */
        if (parent)
            fm_main_win_add_tab(FM_MAIN_WIN(parent), path);
        else
            fm_main_win_add_win(NULL, path);
        fm_path_unref(path);
    }
    gtk_widget_destroy(GTK_WIDGET(dlg->dlg));
    g_slice_free(ConnectDlg, dlg);
    pcmanfm_unref();
}

static void on_server_type(GtkComboBox *box, ConnectDlg *dlg)
{
    GtkTreeIter iter;
    char *scheme;

    if (!gtk_combo_box_get_active_iter(box, &iter))
        return;
    gtk_tree_model_get(gtk_combo_box_get_model(box), &iter, 1, &scheme, -1);
    if (scheme == NULL)
        return;
    if (strcmp(scheme, "sftp") == 0)
    {
        /* disable anonymous */
        gtk_widget_set_sensitive(GTK_WIDGET(dlg->user_anonymous), FALSE);
        gtk_toggle_button_set_active(dlg->user_user, TRUE);
        /* set user to user's login */
        gtk_entry_set_text(dlg->login_entry, g_get_user_name());
        /* set port to 22 */
        gtk_spin_button_set_value(dlg->server_port, 22.0);
    }
    else if (strcmp(scheme, "ftp") == 0)
    {
        /* enable anonymous and set default */
        gtk_widget_set_sensitive(GTK_WIDGET(dlg->user_anonymous), TRUE);
        gtk_toggle_button_set_active(dlg->user_anonymous, TRUE);
        /* set port to 21 */
        gtk_spin_button_set_value(dlg->server_port, 21.0);
    }
    else if (strcmp(scheme, "dav") == 0)
    {
        /* enable anonymous */
        gtk_widget_set_sensitive(GTK_WIDGET(dlg->user_anonymous), TRUE);
        /* set port to 80 */
        gtk_spin_button_set_value(dlg->server_port, 80.0);
    }
    g_free(scheme);
}

static void on_server_host(GtkEditable *editable, ConnectDlg *dlg)
{
    const char *text = gtk_entry_get_text(GTK_ENTRY(editable));
    gboolean ready;

    /* disable OK if entry is empty */
    if (!text || !text[0])
        ready = FALSE;
    /* disable OK if type not selected */
    else if (gtk_combo_box_get_active(dlg->server_type) == -1)
        ready = FALSE;
    /* enable OK if anonymous is enabled */
    else if (gtk_toggle_button_get_active(dlg->user_anonymous))
        ready = TRUE;
    /* enable OK if login isn't empty */
    else
    {
        text = gtk_entry_get_text(dlg->login_entry);
        ready = (text && text[0] != '\0');
    }
    gtk_widget_set_sensitive(dlg->ok, ready);
}

static void on_user_type(GtkToggleButton *user_anonymous, ConnectDlg *dlg)
{
    gboolean anonymous = gtk_toggle_button_get_active(user_anonymous);
    gboolean ready;
    const char *text;

    if (anonymous)
    {
        /* disable login_entry */
        gtk_widget_set_sensitive(GTK_WIDGET(dlg->login_entry), FALSE);
        /* enable OK if host isn't empty and type is selected */
        text = gtk_entry_get_text(dlg->server_host);
        if (!text || !text[0])
            ready = FALSE;
        else
            ready = (gtk_combo_box_get_active(dlg->server_type) != -1);
    }
    else
    {
        /* enable login_entry */
        gtk_widget_set_sensitive(GTK_WIDGET(dlg->login_entry), TRUE);
        /* enable OK if both host and login aren't empty and type is selected */
        text = gtk_entry_get_text(dlg->server_host);
        if (!text || !text[0])
            ready = FALSE;
        else if (gtk_combo_box_get_active(dlg->server_type) == -1)
            ready = FALSE;
        else
        {
            text = gtk_entry_get_text(dlg->login_entry);
            if (!text || !text[0])
                ready = FALSE;
            else
                ready = TRUE;
        }
    }
    gtk_widget_set_sensitive(dlg->ok, ready);
}

void open_connect_dialog(GtkWindow *parent)
{
    GtkBuilder *builder = gtk_builder_new();
    ConnectDlg *dlg = g_slice_new(ConnectDlg);

    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/connect.ui", NULL);
    dlg->dlg = GTK_WINDOW(gtk_builder_get_object(builder, "dlg"));
    g_signal_connect(dlg->dlg, "response", G_CALLBACK(on_response), dlg);
    dlg->ok = GTK_WIDGET(gtk_builder_get_object(builder, "ok"));
    gtk_widget_set_sensitive((GtkWidget*)dlg->ok, FALSE);
    dlg->server_type = GTK_COMBO_BOX(gtk_builder_get_object(builder, "server_type"));
    g_signal_connect(dlg->server_type, "changed", G_CALLBACK(on_server_type), dlg);
    dlg->server_host = GTK_ENTRY(gtk_builder_get_object(builder, "server_host"));
    g_signal_connect(dlg->server_host, "changed", G_CALLBACK(on_server_host), dlg);
    dlg->server_port = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "server_port"));
    dlg->server_path = GTK_ENTRY(gtk_builder_get_object(builder, "server_path"));
    dlg->user_anonymous = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "user_anonymous"));
    g_signal_connect(dlg->user_anonymous, "toggled", G_CALLBACK(on_user_type), dlg);
    dlg->user_user = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "user_user"));
    dlg->login_entry = GTK_ENTRY(gtk_builder_get_object(builder, "login_entry"));
    g_object_unref(builder);

    pcmanfm_ref();
    if(parent)
        gtk_window_set_transient_for(dlg->dlg, parent);
    gtk_window_present(dlg->dlg);
}
