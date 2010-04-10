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

#include <libfm/fm-gtk.h>
#include "app-config.h"
#include "main-win.h"
#include "desktop.h"
#include "volume-manager.h"
#include "pref.h"
#include "pcmanfm.h"

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
static char* ipc_cwd = NULL;

static int n_pcmanfm_ref = 0;

static GOptionEntry opt_entries[] =
{
    { "new-tab", 't', 0, G_OPTION_ARG_NONE, &new_tab, N_("Open folders in new tabs of the last used window instead of creating new windows"), NULL },
    { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile, N_("Name of configuration profile"), "<profile name>" },
    { "desktop", '\0', 0, G_OPTION_ARG_NONE, &show_desktop, N_("Launch desktop manager"), NULL },
    { "desktop-off", '\0', 0, G_OPTION_ARG_NONE, &desktop_off, N_("Turn off desktop manager if it's running"), NULL },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run PCManFM as a daemon"), NULL },
    { "desktop-pref", '\0', 0, G_OPTION_ARG_NONE, &desktop_pref, N_("Open desktop preference dialog"), NULL },
    { "set-wallpaper", 'w', 0, G_OPTION_ARG_FILENAME, &set_wallpaper, N_("Set desktop wallpaper"), N_("<image file>") },
    { "show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Open preference dialog. 'n' is number of the page you want to show (1, 2, 3...)."), "n" },
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
    fm_app_config_load_from_file(FM_APP_CONFIG(config), config_name);

	fm_gtk_init(config);

    /* the main part */
    if(pcmanfm_run())
    {
        fm_volume_manager_init();
    	gtk_main();
        if(desktop_running)
            fm_desktop_manager_finalize();
        fm_config_save(config, NULL); /* save libfm config */
        fm_app_config_save((FmAppConfig*)config, config_name); /* save pcmanfm config */
        fm_volume_manager_finalize();
    }
    single_instance_finalize();

    fm_gtk_finalize();
    g_object_unref(config);
	return 0;
}

inline static GString* args_to_ipc_buf()
{
    int i;
    GString* buf = g_string_sized_new(1024);
    /* send our current working dir to existing instance via IPC. */
    ipc_cwd = g_get_current_dir();
    g_string_append(buf, ipc_cwd);
    g_string_append_c(buf, '\0');
    g_free(ipc_cwd);
    ipc_cwd = NULL;
    for(i = 0; i < G_N_ELEMENTS(opt_entries)-1;++i)
    {
        GOptionEntry* ent = &opt_entries[i];
        if(G_LIKELY(*ent->long_name))
            g_string_append(buf, ent->long_name);
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
                        char* tmp = fm_canonicalize_filename(*files, TRUE);
                        g_string_append(buf, tmp);
                        g_free(tmp);
                        g_string_append_c(buf, '\0');
                    }
                }
                else
                    g_string_append_c(buf, '\0');
                g_string_append_c(buf, '\0'); /* end of array */
            }
            break;
        case G_OPTION_ARG_FILENAME:
        case G_OPTION_ARG_STRING:   /* string */
            if(*(gchar**)ent->arg_data)
            {
                /* FIXME: Handle . and ..*/
                const char* fn = *(gchar**)ent->arg_data;
                g_string_append(buf, *(gchar**)ent->arg_data);
            }
            break;
        }
        g_string_append_c(buf, '\0');
    }
    g_string_append_c(buf, '\0'); /* EOF */
    return buf;
}

inline static void ipc_buf_to_args(GString* buf)
{
    char* p;
    GHashTable* hash = g_hash_table_new(g_str_hash, g_str_equal);
    int i;
    for(i = 0; i < G_N_ELEMENTS(opt_entries)-1;++i)
        g_hash_table_insert(hash, opt_entries[i].long_name, &opt_entries[i]);
    p = buf->str; /* the fist string in buf is cwd */
    i = strlen(p) + 1;
    ipc_cwd = g_memdup(p, i);
    for( p += i; *p; )
    {
        GOptionEntry* ent;
        char *name = p;
        char* val = strchr(p, '=');
        *val = '\0';
        ++val;
        p = val + strlen(val) + 1; /* next item */
        ent = g_hash_table_lookup(hash, name);
        if(G_LIKELY(ent))
        {
            switch(ent->arg)
            {
            case G_OPTION_ARG_NONE: /* bool */
                *(gboolean*)ent->arg_data = val[0] == '1';
                break;
            case G_OPTION_ARG_INT: /* int */
                *(gint*)ent->arg_data = atoi(val);
                break;
            case G_OPTION_ARG_FILENAME_ARRAY: /* string array */
            case G_OPTION_ARG_STRING_ARRAY:
                {
                    char*** pstrs = (char***)ent->arg_data;
                    if(*pstrs)
                        g_strfreev(*pstrs);
                    if(val && *val) /* the array is not empty */
                    {
                        GPtrArray* strs = g_ptr_array_new();
                        do
                        {
                            g_ptr_array_add(strs, g_strdup(val));
                            val += (strlen(val) + 1);
                        }while(*val != '\0');
                        g_ptr_array_add(strs, NULL);
                        *pstrs = g_ptr_array_free(strs, FALSE);
                        p = val - 1;
                    }
                    else /* the array is empty */
                    {
                        p = val + 1;
                        *pstrs = NULL;
                    }
                    continue;
                }
                break;
            case G_OPTION_ARG_FILENAME:
            case G_OPTION_ARG_STRING: /* string */
                if(*(char**)ent->arg_data)
                    g_free(*(char**)ent->arg_data);
                *(char**)ent->arg_data = *val ? g_strdup(val) : NULL;
                break;
            }
        }
    }
    g_hash_table_destroy(hash);
}

gboolean on_socket_event( GIOChannel* ioc, GIOCondition cond, gpointer data )
{
    int client, r;
    socklen_t addr_len = 0;
    struct sockaddr_un client_addr ={ 0 };
    static char buf[ 1024 ];
    GString* args;

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
            ipc_buf_to_args(args);
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
        GString* buf = args_to_ipc_buf();
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
                fm_config_emit_changed(FM_CONFIG(app_config), "wallpaper");
                fm_app_config_save(app_config, config_name);
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
        if(files_to_open)
        {
            char** filename;
            FmJob* job = fm_file_info_job_new(NULL, 0);
            FmPath* cwd = NULL;
            GList* infos;
            for(filename=files_to_open; *filename; ++filename)
            {
                FmPath* path;
                if( **filename == '/' || strstr(*filename, ":/") ) /* absolute path or URI */
                    path = fm_path_new(*filename);
                else if( strcmp(*filename, "~") == 0 ) /* special case for home dir */
                {
                    path = fm_path_get_home();
                    fm_main_win_add_win(NULL, path);
                    continue;
                }
                else /* basename */
                {
                    if(G_UNLIKELY(!cwd))
                    {
                        /* FIXME: This won't work if those filenames are passed via IPC since the receiving process has different cwd. */
                        char* cwd_str = g_get_current_dir();
                        cwd = fm_path_new(cwd_str);
                        g_free(cwd_str);
                    }
                    path = fm_path_new_relative(cwd, *filename);
                }
                fm_file_info_job_add(FM_FILE_INFO_JOB(job), path);
                fm_path_unref(path);
            }
            if(cwd)
                fm_path_unref(cwd);
            fm_job_run_sync_with_mainloop(job);
            infos = fm_list_peek_head_link(FM_FILE_INFO_JOB(job)->file_infos);
            fm_launch_files_simple(NULL, NULL, infos, pcmanfm_open_folder, NULL);
            g_object_unref(job);
            ret = (n_pcmanfm_ref >= 1); /* if there is opened window, return true to run the main loop. */
        }
        else
        {
            FmPath* path;
            char* cwd = ipc_cwd ? ipc_cwd : g_get_current_dir();
            path = fm_path_new(cwd);
            fm_main_win_add_win(NULL, path);
            fm_path_unref(path);
            g_free(cwd);
            ipc_cwd = NULL;
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

gboolean pcmanfm_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    GList* l = folder_infos;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_open_in_last_active(fi->path);
    }
    return TRUE;
}

void pcmanfm_save_config()
{
    fm_config_save(fm_config, NULL);
    fm_app_config_save(app_config, config_name);
}

void pcmanfm_open_folder_in_terminal(GtkWindow* parent, FmPath* dir)
{
    GAppInfo* app;
    char** argv;
    int argc;
    if(!fm_config->terminal)
    {
        fm_show_error(parent, _("Terminal emulator is not set."));
        fm_edit_preference(parent, PREF_ADVANCED);
        return;
    }
    if(!g_shell_parse_argv(fm_config->terminal, &argc, &argv, NULL))
        return;
    app = g_app_info_create_from_commandline(argv[0], NULL, 0, NULL);
    g_strfreev(argv);
    if(app)
    {
        GError* err = NULL;
        GAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* cwd_str;

        if(fm_path_is_native(dir))
            cwd_str = fm_path_to_str(dir);
        else
        {
            GFile* gf = fm_path_to_gfile(dir);
            cwd_str = g_file_get_path(gf);
            g_object_unref(gf);
        }
        gdk_app_launch_context_set_screen(GDK_APP_LAUNCH_CONTEXT(ctx), parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(GDK_APP_LAUNCH_CONTEXT(ctx), gtk_get_current_event_time());
        g_chdir(cwd_str); /* FIXME: currently we don't have better way for this. maybe a wrapper script? */
        g_free(cwd_str);
        if(!g_app_info_launch(app, NULL, ctx, &err))
        {
            fm_show_error(parent, err->message);
            g_error_free(err);
        }
        g_object_unref(ctx);
        g_object_unref(app);
    }
}

/* FIXME: Need to load content of ~/Templates and list available templates in popup menus. */
void pcmanfm_create_new(GtkWindow* parent, FmPath* cwd, const char* templ)
{
    GError* err = NULL;
    FmPath* dest;
    char* basename;
_retry:
    basename = fm_get_user_input(parent, _("Create New..."), _("Enter a name for the newly created file:"), _("New"));
    if(!basename)
        return;

    dest = fm_path_new_child(cwd, basename);
    g_free(basename);

    if( templ == TEMPL_NAME_FOLDER )
    {
        GFile* gf = fm_path_to_gfile(dest);
        if(!g_file_make_directory(gf, NULL, &err))
        {
            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(parent, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else if( templ == TEMPL_NAME_BLANK )
    {
        GFile* gf = fm_path_to_gfile(dest);
        GFileOutputStream* f = g_file_create(gf, G_FILE_CREATE_NONE, NULL, &err);
        if(f)
        {
            g_output_stream_close(G_OUTPUT_STREAM(f), NULL, NULL);
            g_object_unref(f);
        }
        else
        {
            if(err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(parent, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its
             * folder's monitor and after FmFolder handles it in idle
             * handler. So, we cannot select it since it's not yet in
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else /* templates in ~/Templates */
    {
        FmPath* dir = fm_path_new(g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES));
        FmPath* template = fm_path_new_child(dir, templ);
        fm_copy_file(template, cwd);
        fm_path_unref(template);
    }
    fm_path_unref(dest);
}
