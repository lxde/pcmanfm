/*
 *      single-inst.c: simple IPC mechanism for single instance app
 *
 *      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "single-inst.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

typedef struct _SingleInstClient SingleInstClient;
struct _SingleInstClient
{
    GIOChannel* channel;
    char* cwd;
    int screen_num;
    GPtrArray* argv;
    guint watch;
};

static int sock = -1;
static GIOChannel* io_channel = NULL;
static guint io_watch = 0;
static char* prog_name = NULL;

static GList* clients;
static SingleInstCallback callback = NULL;
static GOptionEntry* opt_entries = NULL;
static int screen_num = 0;

static void get_socket_name(char* buf, int len);
static gboolean on_server_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer data);
static gboolean on_client_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer user_data);

static void single_inst_client_free(SingleInstClient* client)
{
    g_io_channel_shutdown(client->channel, FALSE, NULL);
    g_io_channel_unref(client->channel);
    g_source_remove(client->watch);
    g_free(client->cwd);
    g_ptr_array_foreach(client->argv, (GFunc)g_free, NULL);
    g_ptr_array_free(client->argv, TRUE);
    g_slice_free(SingleInstClient, client);
    /* g_debug("free client"); */
}

static void pass_args_to_existing_instance()
{
    GOptionEntry* ent;
    FILE* f = fdopen(sock, "w");
    char* escaped;

    /* pass cwd */
    char* cwd = g_get_current_dir();
    escaped = g_strescape(cwd, NULL);
    fprintf(f, "%s\n", escaped);
    g_free(escaped);
    g_free(cwd);

    /* pass screen number */
    fprintf(f, "%d\n", screen_num);

    for(ent = opt_entries; ent->long_name; ++ent)
    {
        switch(ent->arg)
        {
        case G_OPTION_ARG_NONE:
            if(*(gboolean*)ent->arg_data)
                fprintf(f, "--%s\n", ent->long_name);
            break;
        case G_OPTION_ARG_STRING:
        case G_OPTION_ARG_FILENAME:
        {
            char* str = *(char**)ent->arg_data;
            if(str && *str)
            {
                fprintf(f, "--%s\n", ent->long_name);
                if(g_str_has_prefix(str, "--")) /* strings begining with -- */
                    fprintf(f, "--\n"); /* prepend a -- to it */
                escaped = g_strescape(str, NULL);
                fprintf(f, "%s\n", escaped);
                g_free(escaped);
            }
            break;
        }
        case G_OPTION_ARG_INT:
            fprintf(f, "--%s\n%d\n", ent->long_name, *(gint*)ent->arg_data);
            break;
        case G_OPTION_ARG_STRING_ARRAY:
        case G_OPTION_ARG_FILENAME_ARRAY:
        {
            char** strv = *(char***)ent->arg_data;
            if(strv && *strv)
            {
                if(*ent->long_name) /* G_OPTION_REMAINING = "" */
                    fprintf(f, "--%s\n", ent->long_name);
                for(; *strv; ++strv)
                {
                    char* str = *strv;
                    if(g_str_has_prefix(str, "--")) /* strings begining with -- */
                        fprintf(f, "--\n"); /* prepend a -- to it */
                    escaped = g_strescape(str, NULL);
                    fprintf(f, "%s\n", escaped);
                    g_free(escaped);
                }
            }
            break;
        }
        case G_OPTION_ARG_DOUBLE:
            fprintf(f, "--%s\n%lf\n", ent->long_name, *(gdouble*)ent->arg_data);
            break;
        case G_OPTION_ARG_INT64:
            fprintf(f, "--%s\n%lld\n", ent->long_name, *(gint64*)ent->arg_data);
            break;
        case G_OPTION_ARG_CALLBACK:
            /* Not supported */
            break;
        }
    }
    fclose(f);
}

SingleInstResult single_inst_init(const char* _prog_name, SingleInstCallback cb, GOptionEntry* _opt_entries, int _screen_num)
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;
    int reuse;

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        return SINGLE_INST_ERROR;

    prog_name = g_strdup(_prog_name);
    opt_entries = _opt_entries;
    screen_num = _screen_num;

    /* FIXME: use abstract socket? */
    addr.sun_family = AF_UNIX;
    get_socket_name(addr.sun_path, sizeof(addr.sun_path));
#ifdef SUN_LEN
    addr_len = SUN_LEN(&addr);
#else
    addr_len = strlen(addr.sun_path) + sizeof(addr.sun_family);
#endif

    /* try to connect to existing instance */
    if(connect(sock, (struct sockaddr*)&addr, addr_len) == 0)
    {
        /* connected successfully, pass args in opt_entries to server process as argv and exit. */
        pass_args_to_existing_instance(_opt_entries);
        return SINGLE_INST_CLIENT;
    }

    /* There is no existing server, and we are in the first instance. */
    unlink(addr.sun_path); /* delete old socket file if it exists. */

    reuse = 1;
    ret = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse) );
    if(bind(sock, (struct sockaddr*)&addr, addr_len) == -1)
    {
        close(sock);
        sock = -1;
        return SINGLE_INST_ERROR;
    }

    callback = cb;

    io_channel = g_io_channel_unix_new(sock);
    g_io_channel_set_encoding(io_channel, NULL, NULL);
    g_io_channel_set_buffered(io_channel, FALSE);

    if(listen(sock, 5) == -1)
    {
        single_inst_finalize();
        return SINGLE_INST_ERROR;
    }
    io_watch = g_io_add_watch(io_channel, G_IO_IN|G_IO_ERR|G_IO_PRI|G_IO_HUP, (GIOFunc)on_server_socket_event, NULL);
    return SINGLE_INST_SERVER;
}

void single_inst_finalize()
{
    if(sock >=0 )
    {
        close(sock);
        sock = -1;

        if(io_channel)
        {
            char sock_path[256];

            /* disconnect all clients */
            if(clients)
            {
                g_list_foreach(clients, (GFunc)single_inst_client_free, NULL);
                g_list_free(clients);
                clients = NULL;
            }

            if(io_watch)
            {
                g_source_remove(io_watch);
                io_watch = 0;
            }
            g_io_channel_unref(io_channel);
            io_channel = NULL;
            /* remove the file */
            get_socket_name(sock_path, 256);
            unlink(sock_path);
            callback = NULL;
            opt_entries = NULL;
        }
    }
    g_free(prog_name);
    prog_name = NULL;
}

static inline void parse_args(SingleInstClient* client)
{
    GOptionContext* ctx = g_option_context_new("");
    int argc = client->argv->len;
    char** argv = g_new(char*, argc + 1);
    memcpy(argv, client->argv->pdata, sizeof(char*) * argc);
    argv[argc] = NULL;
    g_option_context_add_main_entries(ctx, opt_entries, NULL);
    g_option_context_parse(ctx, &argc, &argv, NULL);
    g_free(argv);
    g_option_context_free(ctx);
    if(callback)
        callback(client->cwd, client->screen_num);
}

gboolean on_client_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer user_data)
{
    SingleInstClient* client = (SingleInstClient*)user_data;

    if ( cond & (G_IO_IN|G_IO_PRI) )
    {
        char *line;
        gsize term;

        while(g_io_channel_read_line(ioc, &line, NULL, &term, NULL) == G_IO_STATUS_NORMAL)
        {
            if(line)
            {
                line[term] = '\0';
                g_debug("line = %s", line);
                if(!client->cwd)
                    client->cwd = g_strcompress(line);
                else if(client->screen_num == -1)
                {
                    client->screen_num = atoi(line);
                    if(client->screen_num < 0)
                        client->screen_num = 0;
                }
                else
                {
                    char* str = g_strcompress(line);
                    g_ptr_array_add(client->argv, str);
                }
                g_free(line);
            }
        }
    }

    if(cond & (G_IO_ERR|G_IO_HUP))
    {
        if(! (cond & G_IO_ERR) ) /* if there is no error */
        {
            /* try to parse argv */
            parse_args(client);
        }
        single_inst_client_free(client);
        clients = g_list_remove(clients, client);
        return FALSE;
    }

    return TRUE;
}

gboolean on_server_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer data)
{
    if ( cond & (G_IO_IN|G_IO_PRI) )
    {
        int client_sock = accept(g_io_channel_unix_get_fd(ioc), NULL, 0);
        if(client_sock != -1)
        {
            SingleInstClient* client = g_slice_new0(SingleInstClient);
            client->channel = g_io_channel_unix_new(client_sock);
            g_io_channel_set_encoding(client->channel, NULL, NULL);
            client->screen_num = -1;
            client->argv = g_ptr_array_new();
            g_ptr_array_add(client->argv, g_strdup(g_get_prgname()));
            client->watch = g_io_add_watch(client->channel, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP,
                                           on_client_socket_event, client);
            clients = g_list_prepend(clients, client);
            /* g_debug("accept new client"); */
        }
        else
            g_debug("accept() failed!\n%s", g_strerror(errno));
    }

    if(cond & (G_IO_ERR|G_IO_HUP))
    {
        char* _prog_name = prog_name;
        prog_name = NULL;
        single_inst_finalize();
        single_inst_init(_prog_name, callback, opt_entries, screen_num);
        g_free(_prog_name);
        return FALSE;
    }

    return TRUE;
}

void get_socket_name(char* buf, int len)
{
    const char* dpy = g_getenv("DISPLAY");
    char* host = NULL;
    int dpynum;
    if(dpy)
    {
        const char* p = strrchr(dpy, ':');
        host = g_strndup(dpy, (p - dpy));
        dpynum = atoi(p + 1);
    }
    else
        dpynum = 0;
    g_snprintf(buf, len, "%s/.%s-socket-%s-%d-%s",
                g_get_tmp_dir(),
                prog_name,
                host ? host : "",
                dpynum,
                g_get_user_name());
}

