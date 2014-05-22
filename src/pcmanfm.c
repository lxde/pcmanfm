/*
 *      pcmanfm.c
 *
 *      Copyright 2009 - 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
static gboolean daemon_mode = FALSE;
static gboolean first_run = TRUE;
static guint save_config_idle = 0;

static char** files_to_open = NULL;
static int n_files_to_open = 0;
static char* profile = NULL;
static gboolean no_desktop = FALSE;
static gboolean show_desktop = FALSE;
static gboolean desktop_off = FALSE;
static gboolean desktop_running = FALSE;
static gboolean one_screen = FALSE;
/* static gboolean new_tab = FALSE; */
static gint show_pref = -1;
static gboolean desktop_pref = FALSE;
static char* set_wallpaper = NULL;
static char* wallpaper_mode = NULL;
static gboolean new_win = FALSE;
#if FM_CHECK_VERSION(1, 0, 2)
static gboolean find_files = FALSE;
#endif
static char* ipc_cwd = NULL;
static char* window_role = NULL;

static int n_pcmanfm_ref = 0;

static GOptionEntry opt_entries[] =
{
    /* options only acceptable by first pcmanfm instance. These options are not passed through IPC */
    { "profile", 'p', 0, G_OPTION_ARG_STRING, &profile, N_("Name of configuration profile"), N_("PROFILE") },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run PCManFM as a daemon"), NULL },
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("No function. Just to be compatible with nautilus"), NULL },

    /* options that are acceptable for every instance of pcmanfm and will be passed through IPC. */
    { "desktop", '\0', 0, G_OPTION_ARG_NONE, &show_desktop, N_("Launch desktop manager"), NULL },
    { "desktop-off", '\0', 0, G_OPTION_ARG_NONE, &desktop_off, N_("Turn off desktop manager if it's running"), NULL },
    { "desktop-pref", '\0', 0, G_OPTION_ARG_NONE, &desktop_pref, N_("Open desktop preference dialog"), NULL },
    { "one-screen", '\0', 0, G_OPTION_ARG_NONE, &one_screen, N_("Use --desktop option only for one screen"), NULL },
    { "set-wallpaper", 'w', 0, G_OPTION_ARG_FILENAME, &set_wallpaper, N_("Set desktop wallpaper from image FILE"), N_("FILE") },
                    /* don't translate list of modes in description, please */
    { "wallpaper-mode", '\0', 0, G_OPTION_ARG_STRING, &wallpaper_mode, N_("Set mode of desktop wallpaper. MODE=(color|stretch|fit|crop|center|tile|screen)"), N_("MODE") },
    { "show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Open Preferences dialog on the page N"), N_("N") },
    { "new-win", 'n', 0, G_OPTION_ARG_NONE, &new_win, N_("Open new window"), NULL },
#if FM_CHECK_VERSION(1, 0, 2)
    { "find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Open a Find Files window"), NULL },
#endif
    { "role", '\0', 0, G_OPTION_ARG_STRING, &window_role, N_("Window role for usage by window manager"), N_("ROLE") },
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files_to_open, NULL, N_("[FILE1, FILE2,...]")},
    { NULL }
};

static gboolean pcmanfm_run(gint screen_num);

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
                g_free(scheme);
            }
            else /* a file path */
            {
                files_to_open[i] = fm_canonicalize_filename(file, cwd);
                g_free(file);
            }
        }
    }
    pcmanfm_run(screen_num);
    window_role = NULL; /* reset it for clients callbacks */
}

#if FM_CHECK_VERSION(1, 2, 0)
/* ---- statusbar plugins support ---- */
FM_MODULE_DEFINE_TYPE(tab_page_status, FmTabPageStatusInit, 1)

GList *_tab_page_modules = NULL;

static gboolean fm_module_callback_tab_page_status(const char *name, gpointer init, int ver)
{
    /* add module callbacks into own data list */
    if (((FmTabPageStatusInit*)init)->sel_message == NULL)
        return FALSE;
    if (((FmTabPageStatusInit*)init)->init && !((FmTabPageStatusInit*)init)->init())
        return FALSE;
    _tab_page_modules = g_list_append(_tab_page_modules, init);
    return TRUE;
}
#endif

static void on_config_changed(FmAppConfig *cfg, gpointer _unused)
{
    pcmanfm_save_config(FALSE);
}

int main(int argc, char** argv)
{
    FmConfig* config;
    GError* err = NULL;
    SingleInstData inst;
#if FM_CHECK_VERSION(1, 2, 0)
    GList *l;
#endif

#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    /* initialize GTK+ and parse the command line arguments */
    if(G_UNLIKELY(!gtk_init_with_args(&argc, &argv, " ", opt_entries, GETTEXT_PACKAGE, &err)))
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

    fm_gtk_init(config);

#if FM_CHECK_VERSION(1, 2, 0)
    /* register our modules */
    fm_modules_add_directory(PACKAGE_MODULES_DIR);
    fm_module_register_tab_page_status();
#endif

#if FM_CHECK_VERSION(1, 0, 2)
#  if !FM_CHECK_VERSION(1, 2, 0)
    /* the sort_by value isn't loaded with LibFM 1.1.x so we need a workaround */
    /* a little trick to initialize FmFolderModel class */
    gpointer null_model = fm_folder_model_new(NULL, FALSE);
    g_object_unref(null_model);
#  endif
#endif

    /* load pcmanfm-specific config file */
    fm_app_config_load_from_profile(FM_APP_CONFIG(config), profile);
    g_signal_connect(config, "changed::saved_search", G_CALLBACK(on_config_changed), NULL);

    /* the main part */
    if(pcmanfm_run(gdk_screen_get_number(gdk_screen_get_default())))
    {
        first_run = FALSE;
        window_role = NULL; /* reset it for clients callbacks */
        fm_volume_manager_init();
#if !GTK_CHECK_VERSION(3, 6, 0)
        GDK_THREADS_ENTER();
#endif
        gtk_main();
        /* g_debug("main loop ended"); */
#if !GTK_CHECK_VERSION(3, 6, 0)
        GDK_THREADS_LEAVE();
#endif

        if(save_config_idle)
        {
            pcmanfm_save_config(TRUE);
            g_source_remove(save_config_idle);
            save_config_idle = 0;
        }
        fm_volume_manager_finalize();
    }

#if FM_CHECK_VERSION(1, 2, 0)
    for (l = _tab_page_modules; l; l = l->next)
        if (((FmTabPageStatusInit*)l->data)->finalize)
            ((FmTabPageStatusInit*)l->data)->finalize();
    fm_module_unregister_type("tab_page_status");
    g_list_free(_tab_page_modules);
    _tab_page_modules = NULL;
#endif

    single_inst_finalize(&inst);
    fm_gtk_finalize();

    g_object_unref(config);
    return 0;
}

gboolean pcmanfm_run(gint screen_num)
{
    FmMainWin *win = NULL;
    gboolean ret = TRUE;

    if(!files_to_open)
    {
        /* FIXME: use screen number from client and pointer position */
        FmDesktop *desktop = fm_desktop_get(screen_num, 0);

        /* Launch desktop manager */
        if(show_desktop)
        {
            if(!desktop_running)
            {
                fm_desktop_manager_init(one_screen ? screen_num : -1);
                desktop_running = TRUE;
                one_screen = FALSE;
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
            fm_edit_preference(GTK_WINDOW(desktop), show_pref - 1);
            show_pref = -1;
            return TRUE;
        }
        else if(desktop == NULL)
        {
            /* ignore desktop-oriented commands if no desktop support */
            if (desktop_pref || wallpaper_mode || set_wallpaper)
            {
                /* FIXME: add "on this X screen/monitor" into diagnostics */
                fm_show_error(NULL, NULL, _("Desktop manager is not active."));
                return FALSE;
            }
        }
        else if(desktop_pref)
        {
            fm_desktop_preference(NULL, desktop);
            desktop_pref = FALSE;
            return TRUE;
        }
        else if(wallpaper_mode || set_wallpaper)
        {
            gboolean wallpaper_changed = FALSE;
            if(set_wallpaper) /* a new wallpaper is assigned */
            {
                /* g_debug("\'%s\'", set_wallpaper); */
                /* Make sure this is a support image file. */
                if(gdk_pixbuf_get_file_info(set_wallpaper, NULL, NULL))
                {
                    if(desktop->conf.wallpaper)
                        g_free(desktop->conf.wallpaper);
                    desktop->conf.wallpaper = set_wallpaper;
                    if(! wallpaper_mode) /* if wallpaper mode is not specified */
                    {
                        /* do not use solid color mode; otherwise wallpaper won't be shown. */
                        if(desktop->conf.wallpaper_mode == FM_WP_COLOR)
                            desktop->conf.wallpaper_mode = FM_WP_FIT;
                    }
                    wallpaper_changed = TRUE;
                }
                else
                    g_free(set_wallpaper);
                set_wallpaper = NULL;
            }

            if(wallpaper_mode)
            {
                FmWallpaperMode mode = fm_app_wallpaper_get_mode_by_name(wallpaper_mode);

                if (mode != (FmWallpaperMode)-1
                    && mode != desktop->conf.wallpaper_mode)
                {
                    desktop->conf.wallpaper_mode = mode;
                    wallpaper_changed = TRUE;
                }
                g_free(wallpaper_mode);
                wallpaper_mode = NULL;
            }

            if(wallpaper_changed)
                fm_desktop_wallpaper_changed(desktop);

            return FALSE;
        }
    }

    if(files_to_open)
    {
        char** filename;
        FmPath* cwd = NULL;
        GList* paths = NULL;
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
                win = fm_main_win_add_win(NULL, path);
                if(new_win && window_role)
                    gtk_window_set_role(GTK_WINDOW(win), window_role);
                continue;
            }
            else /* basename */
            {
                if(G_UNLIKELY(!cwd))
                {
                    char* cwd_str = ipc_cwd ? ipc_cwd : g_get_current_dir();
                    cwd = fm_path_new_for_str(cwd_str);
                    g_free(cwd_str);
                }
                path = fm_path_new_relative(cwd, *filename);
            }
            paths = g_list_append(paths, path);
        }
        if(cwd)
            fm_path_unref(cwd);
        fm_launch_paths_simple(NULL, NULL, paths, pcmanfm_open_folder, NULL);
        g_list_foreach(paths, (GFunc)fm_path_unref, NULL);
        g_list_free(paths);
        ret = (n_pcmanfm_ref >= 1); /* if there is opened window, return true to run the main loop. */

        g_strfreev(files_to_open);
        files_to_open = NULL;
    }
    else
    {
        if(first_run && daemon_mode)
        {
            /* If the function is called the first time and we're in daemon mode,
           * don't open any folder.
           * Checking if pcmanfm_run() is called the first time is needed to fix
           * #3397444 - pcmanfm dont show window in daemon mode if i call 'pcmanfm' */
            pcmanfm_ref();
        }
#if FM_CHECK_VERSION(1, 0, 2)
        else if (G_LIKELY(!find_files || n_pcmanfm_ref < 1))
#else
        else
#endif
        {
            /* If we're not in daemon mode, or pcmanfm_run() is called because another
             * instance send signal to us, open cwd by default. */
            FmPath* path;
            char* cwd = ipc_cwd ? ipc_cwd : g_get_current_dir();
            path = fm_path_new_for_path(cwd);
            win = fm_main_win_add_win(NULL, path);
            if(new_win && window_role)
                gtk_window_set_role(GTK_WINDOW(win), window_role);
            fm_path_unref(path);
            g_free(cwd);
            ipc_cwd = NULL;
        }
    }

#if FM_CHECK_VERSION(1, 0, 2)
    /* we got a reference at this point so we can open a search dialog */
    if (ret && find_files)
        fm_launch_search_simple(GTK_WINDOW(win), NULL, NULL,
                                pcmanfm_open_folder, NULL);
    find_files = FALSE;
#endif
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
    if( 0 == n_pcmanfm_ref )
        gtk_main_quit();
}

static void move_window_to_desktop(FmMainWin* win, FmDesktop* desktop)
{
    GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(desktop));
    Atom atom;
    char* atom_name = "_NET_WM_DESKTOP";
    XClientMessageEvent xev;

    gtk_window_set_screen(GTK_WINDOW(win), screen);
    if(!XInternAtoms(gdk_x11_get_default_xdisplay(), &atom_name, 1, False, &atom))
    {
        /* g_debug("cannot get Atom for _NET_WM_DESKTOP"); */
        return;
    }
    xev.type = ClientMessage;
    xev.window = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(win)));
    xev.message_type = atom;
    xev.format = 32;
    xev.data.l[0] = desktop->cur_desktop;
    xev.data.l[1] = 0;
    xev.data.l[2] = 0;
    xev.data.l[3] = 0;
    xev.data.l[4] = 0;
    /* g_debug("moving window to current desktop"); */
    XSendEvent(gdk_x11_get_default_xdisplay(), GDK_ROOT_WINDOW(), False,
               (SubstructureNotifyMask | SubstructureRedirectMask),
               (XEvent *) &xev);
}

gboolean pcmanfm_open_folder(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    GList* l = folder_infos;
    gboolean use_new_win = new_win;

    /* for desktop folder open it in new win if set in config */
    if (!use_new_win && user_data && FM_IS_DESKTOP(user_data))
        use_new_win = app_config->desktop_folder_new_win;
    if(use_new_win)
    {
        FmMainWin *win = fm_main_win_add_win(NULL,
                                fm_file_info_get_path((FmFileInfo*)l->data));
        if(window_role)
            gtk_window_set_role(GTK_WINDOW(win), window_role);
        new_win = FALSE;
        l = l->next;
    }
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
            save_config_idle = gdk_threads_add_idle_full(G_PRIORITY_LOW, (GSourceFunc)on_save_config_idle, NULL, NULL);
    }
}

void pcmanfm_open_folder_in_terminal(GtkWindow* parent, FmPath* dir)
{
#if !FM_CHECK_VERSION(1, 2, 0)
    GAppInfo* app;
    char** argv;
    int argc;
#endif

    if(!fm_config->terminal)
    {
        fm_show_error(parent, NULL, _("Terminal emulator is not set."));
        fm_edit_preference(parent, PREF_ADVANCED);
        return;
    }
#if FM_CHECK_VERSION(1, 2, 0)
    else
    {
#else
    if(!g_shell_parse_argv(fm_config->terminal, &argc, &argv, NULL))
        return;
    app = g_app_info_create_from_commandline(argv[0], NULL, 0, NULL);
    g_strfreev(argv);
    if(app)
    {
        GdkAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* old_cwd = g_get_current_dir();
        const char *old_pwd = g_getenv("PWD");
#endif
        GError* err = NULL;
        char* cwd_str;

        if(fm_path_is_native(dir))
            cwd_str = fm_path_to_str(dir);
        else
        {
            GFile* gf = fm_path_to_gfile(dir);
            cwd_str = g_file_get_path(gf);
            g_object_unref(gf);
        }
#if FM_CHECK_VERSION(1, 2, 0)
        if (!fm_terminal_launch(cwd_str, &err))
#else
        /* this is a bit dirty to manipulate environment but what else to do? */
        g_setenv("PWD", cwd_str, TRUE);
        gdk_app_launch_context_set_screen(ctx, parent ? gtk_widget_get_screen(GTK_WIDGET(parent)) : gdk_screen_get_default());
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        g_chdir(cwd_str); /* FIXME: currently we don't have better way for this. maybe a wrapper script? */
        g_free(cwd_str);

        if(!g_app_info_launch(app, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err))
#endif
        {
            fm_show_error(parent, NULL, err->message);
            g_error_free(err);
        }
#if FM_CHECK_VERSION(1, 2, 0)
        g_free(cwd_str);
#else
        g_object_unref(ctx);
        g_object_unref(app);

        /* switch back to old cwd and fix #3114626 - PCManFM 0.9.9 Umount partitions problem */
        g_chdir(old_cwd); /* This is really dirty, but we don't have better solution now. */
        g_free(old_cwd);
        if (old_pwd)
            g_setenv("PWD", old_pwd, TRUE);
        else
            g_unsetenv("PWD");
#endif
    }
}

char* pcmanfm_get_profile_dir(gboolean create)
{
    char* dir = g_build_filename(g_get_user_config_dir(), "pcmanfm", profile ? profile : "default", NULL);
    if(create)
        g_mkdir_with_parents(dir, 0700);
    return dir;
}
