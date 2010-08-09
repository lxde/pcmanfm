//      single-inst.c: simple IPC mechanism for single instance app
//
//      Copyright 2010 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include "single-inst.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

typedef struct _SingleInstClient SingleInstClient;
struct _SingleInstClient
{
    GIOChannel* channel;
    GByteArray* buffer;
    gint cmd;
    gsize chunk_size;
    guint watch;
};

static int sock = -1;
static GIOChannel* io_channel = NULL;
static guint io_watch = 0;
static char* prog_name = NULL;

static GList* clients;
static SingleInstCallback server_cmd_cb = NULL;

static void get_socket_name(char* buf, int len);
static gboolean on_server_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer data);
static gboolean on_client_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer user_data);

static void single_inst_client_free(SingleInstClient* client)
{
    g_io_channel_shutdown(client->channel, FALSE, NULL);
    g_io_channel_unref(client->channel);
    g_source_remove(client->watch);
    g_byte_array_free(client->buffer, TRUE);
    g_slice_free(SingleInstClient, client);
    /* g_debug("free client"); */
}

SingleInstResult single_inst_init(const char* _prog_name, SingleInstCallback _server_cmd_cb)
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;
    int reuse;

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        return SINGLE_INST_ERROR;

    prog_name = g_strdup(_prog_name);
    server_cmd_cb = _server_cmd_cb;

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
        /* connected successfully */
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
        }
    }
    g_free(prog_name);
    prog_name = NULL;
}

/* Format of data chunk:
 * gsize chunk_size = sizeof(chunk_size) + sizeof(cmd) + sizeof(data_type) + sizeof(data)
 * int cmd;
 * int data_type;
 * char data[n];
 */

static void dispatch_command(char* data, int data_len)
{
    int cmd;
    SingleInstCmdData cmd_data;

    /* read the command id */
    memcpy(&cmd, data, sizeof(cmd));
    data += sizeof(cmd);

    /* read data type */
    memcpy(&cmd_data.type, data, sizeof(cmd_data.type));
    data += sizeof(cmd_data.type);

    /* g_debug("cmd = %d, data_len=%d", cmd, data_len); */
    /* data should points to the real data now */
    cmd_data.len = data_len - sizeof(cmd) - sizeof(cmd_data.type);
    cmd_data.data = data;

    if(server_cmd_cb)
        server_cmd_cb(cmd, &cmd_data);
}

gboolean on_client_socket_event(GIOChannel* ioc, GIOCondition cond, gpointer user_data)
{
    SingleInstClient* client = (SingleInstClient*)user_data;

    if ( cond & (G_IO_IN|G_IO_PRI) )
    {
        char buf[4096];
        int r;
        GIOStatus status;

        _read_again:
        status = g_io_channel_read_chars(ioc, buf, sizeof(buf), &r, NULL);
        if(status == G_IO_STATUS_AGAIN) /* FIXME: how many times should we retry? */
            goto _read_again;
        g_byte_array_append( client->buffer, (guint8*)buf, r);

        /* we don't know the size of data chunk yet. */
        if(G_UNLIKELY(client->chunk_size == 0))
        {
            if(client->buffer->len > 0) /* read from it. */
                memcpy(&client->chunk_size, client->buffer->data, sizeof(gsize));
        }
        while(G_LIKELY(client->chunk_size > 0))
        {
            /* g_debug("chunk_size = %d, buf_len = %d", client->chunk_size, client->buffer->len); */
            if(client->buffer->len >= client->chunk_size)
            {
                /* unpack the data chunk */
                char* data = (char*)client->buffer->data + sizeof(client->chunk_size);
                int data_len = client->chunk_size - sizeof(client->chunk_size);
                dispatch_command(data, data_len);

                g_byte_array_remove_range(client->buffer, 0, client->chunk_size);
                if(client->buffer->len > 0)
                    memcpy(&client->chunk_size, client->buffer->data, sizeof(gsize));
                else
                    client->chunk_size = 0;
            }
            else
                break;
        }
    }

    if(cond & (G_IO_ERR|G_IO_HUP))
    {
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
            g_io_channel_set_buffered(client->channel, FALSE);
            client->buffer = g_byte_array_sized_new(4096);
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
        SingleInstCallback cb = server_cmd_cb;
        prog_name = NULL;
        single_inst_finalize();
        single_inst_init(_prog_name, cb);
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

static GByteArray* send_buf_new(int cmd, SingleInstDataType type, int data_len)
{
    GByteArray* buf = g_byte_array_sized_new(1024);
    gsize chunk_size = sizeof(gsize) + sizeof(cmd) + sizeof(type) + data_len;
    g_byte_array_append(buf, (guint8*)&chunk_size, sizeof(chunk_size));
    g_byte_array_append(buf, (guint8*)&cmd, sizeof(cmd));
    g_byte_array_append(buf, (guint8*)&type, sizeof(type));
    return buf;
}

static void send_and_free_buf(GByteArray* buf)
{
    char* pdata;
    int len;
    pdata = (char*)buf->data;
    len = buf->len;
    while( len > 0)
    {
        int r = write(sock, pdata, len);
        if(r == -1)
            break;
        len -= r;
        pdata += r;
    }
    g_byte_array_free(buf, TRUE);
}

inline static void send_data(int cmd, SingleInstDataType type, gpointer data, int len)
{
    GByteArray* buf = send_buf_new(cmd, type, len);
    if(data)
        g_byte_array_append(buf, (guint8*)data, len);
    send_and_free_buf(buf);
}

void single_inst_send_bytes(int cmd, const char* data, int len)
{
    send_data(cmd, SID_BYTES, data, len);
}

void single_inst_send_str(int cmd, const char* str)
{
    send_data(cmd, SID_STR, str, str ? strlen(str) + 1 : 0);
}

void single_inst_send_strv(int cmd, const char** strv)
{
    GByteArray* buf;
    int old_buf_len = 0;
    gsize chunk_size;
    int i, n;
    buf = send_buf_new(cmd, SID_STRV, 0);
    /* store old chunk size and original buf len */
    memcpy(&chunk_size, buf->data, sizeof(chunk_size));
    old_buf_len = buf->len;

    n = strv ? g_strv_length(strv) : 0;
    /* write number of strings in the vector */
    g_byte_array_append(buf, &n, sizeof(n));
    for( i = 0; i < n; ++i )
    {
        char* str = strv[i];
        int slen = strlen(str) + 1;
        /* write length of the string including '\0' */
        g_byte_array_append(buf, &slen, sizeof(slen));
        g_byte_array_append(buf, str, slen); /* write the string */
    }

    /* update chunk size */
    chunk_size += (buf->len - old_buf_len);
    memcpy(buf->data, &chunk_size, sizeof(chunk_size));

    send_and_free_buf(buf);
}

void single_inst_send_int(int cmd, int val)
{
    send_data(cmd, SID_INT, &val, sizeof(val));
}

void single_inst_send_bool(int cmd, gboolean val)
{
    send_data(cmd, SID_BOOL, &val, sizeof(val));
}

char* single_inst_get_str(SingleInstCmdData* data, int* len)
{
    char* ret;
    if(data->type != SID_STR)
        return NULL;
    if(data->len == 0) /* NULL string */
        return NULL;
    ret = g_new0(char, data->len);
    memcpy(ret, data->data, data->len);
    if(len)
        *len = data->len - 1;
    return ret;
}

char* single_inst_get_strv(SingleInstCmdData* data, int* len)
{
    int i, n;
    char* pdata, *pend;
    int data_len = data->len; /* length of the raw data */

    if(data->type != SID_STRV)
        return NULL;

    pdata = data->data;
    pend = pdata + data->len; /* end address of data chunk */

    /* read number of strings in the vector */
    memcpy(&n, pdata, sizeof(n));
    pdata += sizeof(n);

    if( n > 0)
    {
        char** ret = g_new0(char*, n + 1); /* allocate the array */
        for(i = 0; i < n; ++i)
        {
            int slen;
            char* str;
            memcpy(&slen, pdata, sizeof(slen)); /* read length of strv[i] */
            pdata += sizeof(slen);
            if(pdata + slen > pend) /* out of boundary. an error is found. */
            {
                g_strfreev(ret);
                return NULL;
            }
            str = g_new(char, slen);
            memcpy(str, pdata, slen);
            ret[i] = str;
            pdata += slen; /* jump to next string */
        }
        if(len)
            *len = i;
        return ret;
    }
    return NULL;
}

gboolean single_inst_get_int(SingleInstCmdData* data, int* val)
{
    if(data->type != SID_INT)
        return FALSE;
    memcpy(val, data->data, sizeof(int));
    return TRUE;
}

gboolean single_inst_get_bool(SingleInstCmdData* data, gboolean* val)
{
    if(data->type != SID_BOOL)
        return FALSE;
    memcpy(val, data->data, sizeof(gboolean));
    return TRUE;
}
