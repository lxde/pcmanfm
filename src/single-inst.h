/*
 *      single-inst.h: simple IPC mechanism for single instance app
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

#ifndef __SINGLE_INST_H__
#define __SINGLE_INST_H__

#include <glib.h>

G_BEGIN_DECLS


typedef enum _SingleInstResult  SingleInstResult;

enum _SingleInstResult
{
    SINGLE_INST_SERVER,
    SINGLE_INST_CLIENT,
    SINGLE_INST_ERROR
};

typedef void (*SingleInstCallback)(const char* cwd, int screen);

typedef struct
{
    /* set by caller */
    const char* prog_name;
    SingleInstCallback cb;
    const GOptionEntry* opt_entries;
    int screen_num;
    /* private */
    GIOChannel* io_channel;
    int sock;
    guint io_watch;
} SingleInstData;

SingleInstResult single_inst_init(SingleInstData* data);
void single_inst_finalize(SingleInstData* data);

G_END_DECLS

#endif /* __SINGLE_INST_H__ */
