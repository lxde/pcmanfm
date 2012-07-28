/*
 *      pcmanfm.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include <stdlib.h>
#include <string.h>
/* socket is used to keep single instance */
#include <sys/types.h>
#include <signal.h>
#include <unistd.h> /* for getcwd */

#include <libfm/fm-gtk.h>
#include "app-config.h"
#include "main-win.h"
#include "desktop.h"
#include "volume-manager.h"
#include "pref.h"
#include "pcmanfm.h"
#include "single-inst.h"

static int signal_pipe[2] = {-1, -1};
gboolean daemon_mode = FALSE;
static guint save_config_idle = 0;

static char** files_to_open = NULL;
static int n_files_to_open = 0;
static char* profile = NULL;
static gboolean no_desktop = FALSE;
static gboolean show_desktop = FALSE;
static gboolean desktop_off = FALSE;
static gboolean desktop_running = FALSE;
/* static gboolean new_tab = FALSE; */
static int show_pref = 0;
static gboolean desktop_pref = FALSE;
static char* set_wallpaper = NULL;
static char* wallpaper_mode = NULL;
/* static gboolean new_win = FALSE; */
static gboolean find_files = FALSE;
static char* ipc_cwd = NULL;

static int n_pcmanfm_ref = 0;

static GOptionEntry opt_entries[] =
{
    /* options only acceptable by first pcmanfm instance. These options are not passed through IPC */
    { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile, N_("Name of configuration profile"), "<profile name>" },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run PCManFM as a daemon"), NULL },
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("No function. Just to be compatible with nautilus"), NULL },

    /* options that are acceptable for every instance of pcmanfm and will be passed through IPC. */
    { "desktop", '\0', 0, G_OPTION_ARG_NONE, &show_desktop, N_("Launch desktop manager"), NULL },
    { "desktop-off", '\0', 0, G_OPTION_ARG_NONE, &desktop_off, N_("Turn off desktop manager if it's running"), NULL },
    { "desktop-pref", '\0', 0, G_OPTION_ARG_NONE, &desktop_pref, N_("Open desktop preference dialog"), NULL },
    { "set-wallpaper", 'w', 0, G_OPTION_ARG_FILENAME, &set_wallpaper, N_("Set desktop wallpaper"), N_("<image file>") },
    { "wallpaper-mode", '\0', 0, G_OPTION_ARG_STRING, &wallpaper_mode, N_("Set mode of desktop wallpaper. <mode>=(color|stretch|fit|center|tile)"), N_("<mode>") },
    { "show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Open preference dialog. 'n' is number of the page you want to show (1, 2, 3...)."), "n" },
    /* { "new-win", '\0', 'n', G_OPTION_ARG_NONE, &new_win, N_("Open new window"), NULL }, */
    /* { "find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Open Find Files utility"), NULL }, */
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files_to_open, NULL, N_("[FILE1, FILE2,...]")},
    { NULL }
};

static const char* valid_wallpaper_modes[] = {"color", "stretch", "fit", "center", "tile"};

static gboolean pcmanfm_run();

/* it's not safe to call gtk+ functions in unix signal handler
 * since the process is interrupted here and the state of gtk+ is unpredictable. */
static void unix_signal_handler(int sig_num)
{
    /* postpond the signal handling by using a pipe */
    if (write(signal_pipe[1], &sig_num, sizeof(sig_num)) != sizeof(sig_num)) {
        g_critical("cannot bounce the signal, stop");
        exit(2);
    }
}

static gboolean on_unix_signal(GIOChannel* ch, GIOCondition cond, gpointer user_data)
{
    int sig_num;
    GIOStatus status;
    gsize got;

    while(1)
    {
        status = g_io_channel_read_chars(ch, (gchar*)&sig_num, sizeof(sig_num),
                                         &got, NULL);
        if(status == G_IO_STATUS_AGAIN) /* we read all the pipe */
        {
            g_debug("got G_IO_STATUS_AGAIN");
            return TRUE;
        }
        if(status != G_IO_STATUS_NORMAL || got != sizeof(sig_num)) /* broken pipe */
        {
            g_debug("signal pipe is broken");
            gtk_main_quit();
            return FALSE;
        }
        g_debug("got signal %d from pipe", sig_num);
        switch(sig_num)
        {
        case SIGTERM:
        default:
            gtk_main_quit();
            return FALSE;
        }
    }
    return TRUE;
}

static void single_inst_cb(const char* cwd, int screen_num)
{
    g_free(ipc_cwd);
    ipc_cwd = g_strdup(cwd);

    if(files_to_open)
    {
        int i;
        n_files_to_open = g_strv_length(files_to_open);
        /* canonicalize filename if needed. */
        for(i = 0; i < n_files_to_open; ++i)
        {
            char* file = files_to_open[i];
            char* scheme = g_uri_parse_scheme(file);
            g_debug("file: %s", file);
            if(scheme) /* a valid URI */
            {
                /* FIXME: should we canonicalize URIs? and how about file:///? */
                g_free(scheme);
            }
            else /* a file path */
            {
                files_to_open[i] = fm_canonicalize_filename(file, cwd);
                g_free(file);
            }
        }
    }
    pcmanfm_run();
}

int main(int argc, char** argv)
{
    FmConfig* config;
    GError* err = NULL;
    SingleInstData inst;

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

    /* ensure that there is only one instance of pcmanfm. */
    inst.prog_name = "pcmanfm";
    inst.cb = single_inst_cb;
    inst.opt_entries = opt_entries + 3;
    inst.screen_num = gdk_x11_get_default_screen();
    switch(single_inst_init(&inst))
    {
    case SINGLE_INST_CLIENT: /* we're not the first instance. */
        single_inst_finalize(&inst);
        gdk_notify_startup_complete();
        return 0;
    case SINGLE_INST_ERROR: /* error happened. */
        single_inst_finalize(&inst);
        return 1;
    case SINGLE_INST_SERVER: ; /* FIXME */
    }

    if(pipe(signal_pipe) == 0)
    {
        GIOChannel* ch = g_io_channel_unix_new(signal_pipe[0]);
        g_io_add_watch(ch, G_IO_IN|G_IO_PRI, (GIOFunc)on_unix_signal, NULL);
        g_io_channel_unref(ch);

        /* intercept signals */
        // signal( SIGPIPE, SIG_IGN );
        signal( SIGHUP, unix_signal_handler );
        signal( SIGTERM, unix_signal_handler );
        signal( SIGINT, unix_signal_handler );
    }

    config = fm_app_config_new(); /* this automatically load libfm config file. */

    /* load pcmanfm-specific config file */
    fm_app_config_load_from_profile(FM_APP_CONFIG(config), profile);

    fm_gtk_init(config);
    /* the main part */
    if(pcmanfm_run())
    {
        fm_volume_manager_init();
        gtk_main();
        /* g_debug("main loop ended"); */
        if(desktop_running)
            fm_desktop_manager_finalize();

        pcmanfm_save_config(TRUE);
        if(save_config_idle)
        {
            g_source_remove(save_config_idle);
            save_config_idle = 0;
        }
        fm_volume_manager_finalize();
    }

    single_inst_finalize(&inst);
    fm_gtk_finalize();

    g_object_unref(config);
    return 0;
}

static FmJobErrorAction on_file_info_job_error(FmFileInfoJob* job, GError* err, FmJobErrorSeverity severity, gpointer user_data)
{
    if(err->domain == G_IO_ERROR)
    {
        if(err->code == G_IO_ERROR_NOT_MOUNTED)
        {
            if(fm_mount_path(NULL, fm_file_info_job_get_current(job), TRUE))
                return FM_JOB_RETRY;
        }
        else if(err->code == G_IO_ERROR_FAILED_HANDLED)
            return FM_JOB_CONTINUE;
    }
    fm_show_error(NULL, NULL, err->message);
    return FM_JOB_CONTINUE;
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
            /* FIXME: pass screen number from client */
            fm_edit_preference(GTK_WINDOW(fm_desktop_get(0)), show_pref - 1);
            show_pref = 0;
            return TRUE;
        }
        else if(desktop_pref)
        {
            /* FIXME: pass screen number from client */
            fm_desktop_preference(NULL, GTK_WINDOW(fm_desktop_get(0)));
            desktop_pref = FALSE;
            return TRUE;
        }
        else
        {
            gboolean need_to_exit = (wallpaper_mode || set_wallpaper);
            gboolean wallpaper_changed = FALSE;
            if(set_wallpaper) /* a new wallpaper is assigned */
            {
                /* g_debug("\'%s\'", set_wallpaper); */
                /* Make sure this is a support image file. */
                if(gdk_pixbuf_get_file_info(set_wallpaper, NULL, NULL))
                {
                    if(app_config->wallpaper)
                        g_free(app_config->wallpaper);
                    app_config->wallpaper = set_wallpaper;
                    set_wallpaper = NULL;
                    if(! wallpaper_mode) /* if wallpaper mode is not specified */
                    {
                        /* do not use solid color mode; otherwise wallpaper won't be shown. */
                        if(app_config->wallpaper_mode == FM_WP_COLOR)
                            app_config->wallpaper_mode = FM_WP_FIT;
                    }
                    wallpaper_changed = TRUE;
                }
            }

            if(wallpaper_mode)
            {
                guint i = 0;
                for(i = 0; i < G_N_ELEMENTS(valid_wallpaper_modes); ++i)
                {
                    if(strcmp(valid_wallpaper_modes[i], wallpaper_mode) == 0)
                    {
                        if(i != app_config->wallpaper_mode)
                        {
                            app_config->wallpaper_mode = i;
                            wallpaper_changed = TRUE;
                        }
                        break;
                    }
                }
                g_free(wallpaper_mode);
                wallpaper_mode = NULL;
            }

            if(wallpaper_changed)
            {
                fm_config_emit_changed(FM_CONFIG(app_config), "wallpaper");
                fm_app_config_save_profile(app_config, profile);
            }

            if(need_to_exit)
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
            FmFileInfoJob* job = fm_file_info_job_new(NULL, 0);
            FmPath* cwd = NULL;
            GList* infos;
            for(filename=files_to_open; *filename; ++filename)
            {
                FmPath* path;
                if( **filename == '/') /* absolute path */
                    path = fm_path_new_for_path(*filename);
                else if(strstr(*filename, ":/") ) /* URI */
                    path = fm_path_new_for_uri(*filename);
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
                        /* FIXME: should we use ipc_cwd here? */
                        char* cwd_str = g_get_current_dir();
                        cwd = fm_path_new_for_str(cwd_str);
                        g_free(cwd_str);
                    }
                    path = fm_path_new_relative(cwd, *filename);
                }
                fm_file_info_job_add(job, path);
                fm_path_unref(path);
            }
            if(cwd)
                fm_path_unref(cwd);
            g_signal_connect(job, "error", G_CALLBACK(on_file_info_job_error), NULL);
            fm_job_run_sync_with_mainloop(FM_JOB(job));
            infos = fm_file_info_list_peek_head_link(job->file_infos);
            fm_launch_files_simple(NULL, NULL, infos, pcmanfm_open_folder, NULL);
            g_object_unref(job);
            ret = (n_pcmanfm_ref >= 1); /* if there is opened window, return true to run the main loop. */

            g_strfreev(files_to_open);
            files_to_open = NULL;
        }
        else
        {
            static gboolean first_run = TRUE;
            if(first_run && daemon_mode)
            {
                /* If the function is called the first time and we're in daemon mode,
               * don't open any folder.
               * Checking if pcmanfm_run() is called the first time is needed to fix
               * #3397444 - pcmanfm dont show window in daemon mode if i call 'pcmanfm' */
            }
            else
            {
                /* If we're not in daemon mode, or pcmanfm_run() is called because another
               * instance send signal to us, open cwd by default. */
                FmPath* path;
                char* cwd = ipc_cwd ? ipc_cwd : g_get_current_dir();
                path = fm_path_new_for_path(cwd);
                fm_main_win_add_win(NULL, path);
                fm_path_unref(path);
                g_free(cwd);
                ipc_cwd = NULL;
            }
            first_run = FALSE;
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

static void move_window_to_desktop(FmMainWin* win, FmDesktop* desktop)
{
    GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    Atom atom;
    char* atom_name = "_NET_WM_DESKTOP";
    XClientMessageEvent xev;

    gtk_window_set_screen(GTK_WINDOW(win), screen);
    if(!XInternAtoms(GDK_DISPLAY(), &atom_name, 1, False, &atom))
    {
        /* g_debug("cannot get Atom for _NET_WM_DESKTOP"); */
        return;
    }
    xev.type = ClientMessage;
    xev.window = GDK_WINDOW_XID(GTK_WIDGET(win)->window);
    xev.message_type = atom;
    xev.format = 32;
    xev.data.l[0] = desktop->cur_desktop;
    xev.data.l[1] = 0;
    xev.data.l[2] = 0;
    xev.data.l[3] = 0;
    xev.data.l[4] = 0;
    /* g_debug("moving window to current desktop"); */
    XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(), False,
               (SubstructureNotifyMask | SubstructureRedirectMask),
               (XEvent *) &xev);
}

gboolean pcmanfm_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    GList* l = folder_infos;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_open_in_last_active(fm_file_info_get_path(fi));
    }
    if(user_data && FM_IS_DESKTOP(user_data))
        move_window_to_desktop(fm_main_win_get_last_active(), user_data);
    return TRUE;
}

static gboolean on_save_config_idle(gpointer user_data)
{
    pcmanfm_save_config(TRUE);
    save_config_idle = 0;
    return FALSE;
}

void pcmanfm_save_config(gboolean immediate)
{
    if(immediate)
    {
        fm_config_save(fm_config, NULL);
        fm_app_config_save_profile(app_config, profile);
    }
    else
    {
        /* install an idle handler to save the config file. */
        if( 0 == save_config_idle)
            save_config_idle = g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_save_config_idle, NULL, NULL);
    }
}

void pcmanfm_open_folder_in_terminal(GtkWindow* parent, FmPath* dir)
{
    GAppInfo* app;
    char** argv;
    int argc;
    if(!fm_config->terminal)
    {
        fm_show_error(parent, NULL, _("Terminal emulator is not set."));
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
        GdkAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* cwd_str;
        char* old_cwd = g_get_current_dir();

        if(fm_path_is_native(dir))
            cwd_str = fm_path_to_str(dir);
        else
        {
            GFile* gf = fm_path_to_gfile(dir);
            cwd_str = g_file_get_path(gf);
            g_object_unref(gf);
        }
        gdk_app_launch_context_set_screen(ctx, parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        g_chdir(cwd_str); /* FIXME: currently we don't have better way for this. maybe a wrapper script? */
        g_free(cwd_str);

        if(!g_app_info_launch(app, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err))
        {
            fm_show_error(parent, NULL, err->message);
            g_error_free(err);
        }
        g_object_unref(ctx);
        g_object_unref(app);

        /* switch back to old cwd and fix #3114626 - PCManFM 0.9.9 Umount partitions problem */
        g_chdir(old_cwd); /* This is really dirty, but we don't have better solution now. */
        g_free(old_cwd);
    }
}

/* FIXME: Need to load content of ~/Templates and list available templates in popup menus. */
void pcmanfm_create_new(GtkWindow* parent, FmPath* cwd, const char* templ)
{
    GError* err = NULL;
    FmPath* dest;
    char* basename;
    const char* msg;
    //FmMainWin* win = FM_MAIN_WIN(parent);
_retry:
    if(templ == TEMPL_NAME_FOLDER)
        msg = N_("Enter a name for the newly created folder:");
    else
        msg = N_("Enter a name for the newly created file:");
    basename = fm_get_user_input(parent, _("Create New..."), _(msg), _("New"));
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
            fm_show_error(parent, NULL, err->message);
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
            fm_show_error(parent, NULL, err->message);
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
    else if ( templ == TEMPL_NAME_SHORTCUT )
    {
        /* FIXME: a temp. workaround until ~/Templates support is implemented */
         char buf[256];
         GFile* gf = fm_path_to_gfile(dest);

         if (g_find_program_in_path("lxshortcut"))
         {
            char* path = g_file_get_path(gf);
            int s = snprintf(buf, sizeof(buf), "lxshortcut -i %s", path);
            g_free(path);
            if(s >= (int)sizeof(buf))
                buf[0] = '\0';
         }
         else
         {
             GtkWidget* msg;

             msg = gtk_message_dialog_new( NULL,
                                           0,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           _("Error, lxshortcut not installed") );
             gtk_dialog_run( GTK_DIALOG(msg) );
             gtk_widget_destroy( msg );
         }
         if(buf[0] && !g_spawn_command_line_async(buf, NULL))
            fm_show_error(parent, NULL, _("Failed to start lxshortcut"));
         g_object_unref(gf);
    }
    else /* templates in ~/Templates */
    {
        /* FIXME: need an extended processing with desktop entries support */
        FmPath* dir = fm_path_new_for_str(g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES));
        FmPath* template = fm_path_new_child(dir, templ);
        fm_copy_file(parent, template, cwd);
        fm_path_unref(template);
        fm_path_unref(dir);
    }
    fm_path_unref(dest);
}

char* pcmanfm_get_profile_dir(gboolean create)
{
    char* dir = g_build_filename(g_get_user_config_dir(), "pcmanfm", profile ? profile : "default", NULL);
    if(create)
        g_mkdir_with_parents(dir, 0700);
    return dir;
}
