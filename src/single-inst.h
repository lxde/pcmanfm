//      single-inst.h: simple IPC mechanism for single instance app
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


#ifndef __SINGLE_INST_H__
#define __SINGLE_INST_H__

#include <glib.h>

G_BEGIN_DECLS


typedef enum _SingleInstResult  SingleInstResult;
typedef enum _SingleInstDataType    SingleInstDataType;
typedef struct _SingleInstCmdData  SingleInstCmdData;

enum _SingleInstResult
{
    SINGLE_INST_SERVER,
    SINGLE_INST_CLIENT,
    SINGLE_INST_ERROR
};

enum _SingleInstDataType
{
    SID_INVALID,
    SID_BYTES,
    SID_STR,
    SID_STRV,
    SID_INT,
    SID_BOOL
};

struct _SingleInstCmdData
{
    SingleInstDataType type; /* type of data */
    char* data; /* raw data */
    int len; /* length of data */
};

typedef gboolean (*SingleInstCallback)(int cmd, SingleInstCmdData* data);

SingleInstResult single_inst_init(const char* prog_name, SingleInstCallback _server_cmd_cb);
void single_inst_finalize();

void single_inst_send_bytes(int cmd, const char* data, int len);
void single_inst_send_str(int cmd, const char* str);
void single_inst_send_strv(int cmd, const char** strv);
void single_inst_send_int(int cmd, int val);
void single_inst_send_bool(int cmd, gboolean val);

char* single_inst_get_str(SingleInstCmdData* data, int* len);
char* single_inst_get_strv(SingleInstCmdData* data, int* len);
gboolean single_inst_get_int(SingleInstCmdData* data, int* val);
gboolean single_inst_get_bool(SingleInstCmdData* data, gboolean* val);

G_END_DECLS

#endif /* __SINGLE_INST_H__ */
