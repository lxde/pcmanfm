/*
 *      pcmanfm.c
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
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
/* socket is used to keep single instance */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h> /* for getcwd */

#include <fm-gtk.h>
#include "app-config.h"
#include "main-win.h"
#include "desktop.h"
#include "pref.h"

static int sock;
GIOChannel* io_channel = NULL;

gboolean daemon_mode = FALSE;

static char** files_to_open = NULL;
static char* profile = NULL;
static char* config_name = NULL;
static gboolean no_desktop = FALSE;
static gboolean show_desktop = FALSE;
static gboolean desktop_off = FALSE;
static gboolean desktop_running = FALSE;
static gboolean new_tab = FALSE;
static int show_pref = 0;
static gboolean desktop_pref = FALSE;
static char* set_wallpaper = NULL;
static gboolean find_files = FALSE;

static int n_pcmanfm_ref = 0;

static GOptionEntry opt_entries[] =
{
    { "new-tab", 't', 0, G_OPTION_ARG_NONE, &new_tab, N_("Open folders in new tabs of the last used window instead of creating new windows"), NULL },
    { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile, N_("Name of config profile"), "<profile name>" },
    { "desktop", '\0', 0, G_OPTION_ARG_NONE, &show_desktop, N_("Launch desktop manager"), NULL },
    { "desktop-off", '\0', 0, G_OPTION_ARG_NONE, &desktop_off, N_("Turn off desktop manager if it's running"), NULL },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run PCManFM as a daemon"), NULL },
    { "desktop-pref", '\0', 0, G_OPTION_ARG_NONE, &desktop_pref, N_("Open desktop preference dialog"), NULL },
    { "set-wallpaper", 'w', 0, G_OPTION_ARG_FILENAME, &set_wallpaper, N_("Set desktop wallpaper"), N_("<image file>") },
    { "show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Open preference dialog. 'n' is the number of page you want to show (1, 2, 3...)."), "n" },
    { "find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Open Find Files utility"), NULL },
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("No function. Just to be compatible with nautilus"), NULL },
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files_to_open, NULL, N_("[FILE1, FILE2,...]")},
    { NULL }
};

static gboolean single_instance_check();
static void single_instance_finalize();
static void get_socket_name(char* buf, int len);
static gboolean pcmanfm_run();
static gboolean on_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer data);

int main(int argc, char** argv)
{
    FmConfig* config;
    GError* err = NULL;

#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    /* initialize GTK+ and parse the command line arguments */
    if(G_UNLIKELY(!gtk_init_with_args(&argc, &argv, "", opt_entries, GETTEXT_PACKAGE, &err)))
    {
        g_printf("%s\n", err->message);
        g_error_free(err);
        return 1;
    }

    /* ensure that there is only one instance of pcmanfm.
         if there is an existing instance, command line arguments
         will be passed to the existing instance, and exit() will be called here.  */
    single_instance_check();

    /* intercept signals */
    signal( SIGPIPE, SIG_IGN );
    /* signal( SIGHUP, gtk_main_quit ); */
    signal( SIGINT, gtk_main_quit );
    signal( SIGTERM, gtk_main_quit );

    config = fm_app_config_new(); /* this automatically load libfm config file. */
    /* load pcmanfm-specific config file */
    if(profile)
        config_name = g_strconcat("pcmanfm/", profile, ".conf", NULL);
    fm_app_config_load_from_file(config, config_name);

	fm_gtk_init(config);

    /* the main part */
    if(pcmanfm_run())
    {
    	gtk_main();
        if(desktop_running)
            fm_desktop_manager_finalize();
        fm_config_save(config, NULL); /* save libfm config */
        fm_app_config_save((FmAppConfig*)config, config_name); /* save pcmanfm config */
    }
    single_instance_finalize();

    fm_gtk_finalize();
    g_object_unref(config);
	return 0;
}


inline static GString* args_to_keyfile()
{
    int i;
    GString* buf = g_string_sized_new(1024);
    g_string_assign(buf, "[a]\n");
    for(i = 0; i < G_N_ELEMENTS(opt_entries)-1;++i)
    {
        GOptionEntry* ent = &opt_entries[i];
        if(G_LIKELY(*ent->long_name))
            g_string_append(buf, ent->long_name);
        else /* G_OPTION_REMAINING */
            g_string_append(buf, "@");
        g_string_append_c(buf, '=');
        switch(ent->arg)
        {
        case G_OPTION_ARG_NONE: /* bool */
            g_string_append_c(buf, *(gboolean*)ent->arg_data ? '1' : '0');
            break;
        case G_OPTION_ARG_INT:  /* int */
            g_string_append_printf(buf, "%d", *(gint*)ent->arg_data);
            break;
        case G_OPTION_ARG_FILENAME_ARRAY:   /* string array */
        case G_OPTION_ARG_STRING_ARRAY:
            {
                char** files = *(char***)ent->arg_data;
                if(files)
                {
                    for(;*files;++files)
                    {
                        g_string_append(buf, *files);
                        g_string_append_c(buf, ';');
                    }
                }
            }
            break;
        case G_OPTION_ARG_FILENAME:
        case G_OPTION_ARG_STRING:   /* string */
            if(*(gchar**)ent->arg_data)
                g_string_append(buf, *(gchar**)ent->arg_data);
            break;
        }
        g_string_append_c(buf, '\n');
    }
    return buf;
}

inline static void keyfile_to_args(GString* buf)
{
    GKeyFile* kf = g_key_file_new();
    /* g_debug("\n%s", buf->str); */
    if(g_key_file_load_from_data(kf, buf->str, buf->len, 0, NULL))
    {
        int i;
        char*** pstrs;
        for(i = 0; i < G_N_ELEMENTS(opt_entries)-1;++i)
        {
            GOptionEntry* ent = &opt_entries[i];
            switch(ent->arg)
            {
            case G_OPTION_ARG_NONE: /* bool */
                *(gboolean*)ent->arg_data = g_key_file_get_boolean(kf, "a", ent->long_name, NULL);
                break;
            case G_OPTION_ARG_INT: /* int */
                *(gint*)ent->arg_data = g_key_file_get_integer(kf, "a", ent->long_name, NULL);
                break;
            case G_OPTION_ARG_FILENAME_ARRAY: /* string array */
            case G_OPTION_ARG_STRING_ARRAY:
                pstrs = (char**)ent->arg_data;
                if(*pstrs)
                    g_strfreev(*pstrs);
                *pstrs = g_key_file_get_string_list(kf, "a", *ent->long_name ? *ent->long_name : "@", NULL, NULL);
                if(*pstrs)
                {
                    char** strs = *pstrs;
                    char* str = *strs;
                    if(!str || str[0] == '\0')
                    {
                        g_strfreev(strs);
                        *pstrs = NULL;
                    }
                }
                break;
            case G_OPTION_ARG_FILENAME:
            case G_OPTION_ARG_STRING: /* string */
                if(*(char**)ent->arg_data)
                    g_free(*(char**)ent->arg_data);
                *(char**)ent->arg_data = g_key_file_get_string(kf, "a", ent->long_name, NULL);
                if(*(char**)ent->arg_data && **(char**)ent->arg_data == '\0')
                {
                    g_free(*(char**)ent->arg_data);
                    *(char**)ent->arg_data = NULL;
                }
                break;
            }
        }
    }
    g_key_file_free(kf);
}

gboolean on_socket_event( GIOChannel* ioc, GIOCondition cond, gpointer data )
{
    int client, r;
    socklen_t addr_len = 0;
    struct sockaddr_un client_addr ={ 0 };
    static char buf[ 1024 ];
    GString* args;
    char** file;

    if ( cond & G_IO_IN )
    {
        client = accept( g_io_channel_unix_get_fd( ioc ), (struct sockaddr *)&client_addr, &addr_len );
        if ( client != -1 )
        {
            args = g_string_sized_new(1024);
            while( (r = read( client, buf, sizeof(buf) )) > 0 )
                g_string_append_len( args, buf, r);
            shutdown( client, 2 );
            close( client );
            keyfile_to_args(args);
            g_string_free( args, TRUE );
            pcmanfm_run();
        }
    }
    return TRUE;
}

void get_socket_name( char* buf, int len )
{
    char* dpy = gdk_get_display();
    g_snprintf( buf, len, "/tmp/.pcmanfm2-socket%s-%s", dpy, g_get_user_name() );
    g_free( dpy );
}

gboolean single_instance_check()
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;
    int reuse;

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        ret = 1;
        goto _exit;
    }

    /* FIXME: use abstract socket */
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof( addr.sun_path ));
#ifdef SUN_LEN
    addr_len = SUN_LEN(&addr);
#else
    addr_len = strlen( addr.sun_path ) + sizeof( addr.sun_family );
#endif

    /* try to connect to existing instance */
    if(connect(sock, (struct sockaddr*)&addr, addr_len) == 0)
    {
        /* connected successfully */
        GString* buf = args_to_keyfile();
        write(sock, buf->str, buf->len);
        g_string_free(buf, TRUE);

        shutdown( sock, 2 );
        close( sock );
        ret = 0;
        goto _exit;
    }

    /* There is no existing server, and we are in the first instance. */
    unlink( addr.sun_path ); /* delete old socket file if it exists. */
    reuse = 1;
    ret = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse) );
    if(bind(sock, (struct sockaddr*)&addr, addr_len) == -1)
    {
        ret = 1;
        goto _exit;
    }

    io_channel = g_io_channel_unix_new(sock);
    g_io_channel_set_encoding(io_channel, NULL, NULL);
    g_io_channel_set_buffered(io_channel, FALSE);
    g_io_add_watch(io_channel, G_IO_IN,
                   (GIOFunc)on_socket_event, NULL);
    if(listen(sock, 5) == -1)
    {
        ret = 1;
        goto _exit;
    }
    return TRUE;

_exit:

    gdk_notify_startup_complete();
    exit( ret );
}

void single_instance_finalize()
{
    char lock_file[256];
    shutdown(sock, 2);
    g_io_channel_unref(io_channel);
    close(sock);
    get_socket_name(lock_file, sizeof( lock_file ));
    unlink(lock_file);
}


gboolean pcmanfm_run()
{
    gboolean ret = TRUE;
    char** file;
	GtkWidget* w;

    if(!files_to_open)
    {
        /* Launch desktop manager */
        if(show_desktop)
        {
            if(!desktop_running)
            {
                fm_desktop_manager_init();
                desktop_running = TRUE;
            }
            show_desktop = FALSE;
            return TRUE;
        }
        else if(desktop_off)
        {
            if(desktop_running)
            {
                desktop_running = FALSE;
                fm_desktop_manager_finalize();
            }
            desktop_off = FALSE;
            return FALSE;
        }
        else if(show_pref > 0)
        {
            fm_edit_preference(NULL, show_pref - 1);
            show_pref = 0;
            return TRUE;
        }
        else if(desktop_pref)
        {
            fm_desktop_preference();
            desktop_pref = FALSE;
            return TRUE;
        }
        else if(set_wallpaper)
        {
            /* g_debug("\'%s\'", set_wallpaper); */
            /* Make sure this is a support image file. */
            if(gdk_pixbuf_get_file_info(set_wallpaper, NULL, NULL))
            {
                if(app_config->wallpaper)
                    g_free(app_config->wallpaper);
                app_config->wallpaper = set_wallpaper;
                set_wallpaper = NULL;
                if(app_config->wallpaper_mode == FM_WP_COLOR)
                    app_config->wallpaper_mode = FM_WP_FIT;
                fm_config_emit_changed(app_config, "wallpaper");
                fm_app_config_save(app_config, NULL);
            }
            return FALSE;
        }
    }

    if(G_UNLIKELY(find_files))
    {
        /* FIXME: find files */
    }
    else
    {
        if(!files_to_open)
        {
            FmPath* path;
            w = fm_main_win_new();
            gtk_window_set_default_size(w, app_config->win_width, app_config->win_height);
            gtk_widget_show(w);
            path = fm_path_get_home();
            fm_main_win_chdir(w, path);
        }
    }
    return ret;
}

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref()
{
    ++n_pcmanfm_ref;
    /* g_debug("ref: %d", n_pcmanfm_ref); */
}

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
void pcmanfm_unref()
{
    --n_pcmanfm_ref;
    /* g_debug("unref: %d, daemon_mode=%d, desktop_running=%d", n_pcmanfm_ref, daemon_mode, desktop_running); */
    if( 0 == n_pcmanfm_ref && !daemon_mode && !desktop_running )
        gtk_main_quit();
}
