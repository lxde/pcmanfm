/*
 *      pcmanfm-modules.h
 *
 *      This file is a part of PCManFM package: definitions for modules.
 *      It can be used with LibFM and PCManFM version 1.2.0 or newer.
 *
 *      Copyright 2014 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

#ifndef __PCMANFM_MODULES_H__
#define __PCMANFM_MODULES_H__

#include <libfm/fm.h>

G_BEGIN_DECLS

#define FM_MODULE_tab_page_status_VERSION 1

/**
 * FmTabPageStatusInit:
 * @init: (allow-none): once-done initialization callback
 * @finalize: (allow-none): once-done finalization callback
 * @sel_message: callback to make selection-specific statusbar addition
 *
 * The structure describing callbacks for FmTabPage statusbar update
 * extension specific for some file type - tab_page_status plugins.
 *
 * The @sel_message callback is called when the page statusbar for the
 * selected files is about to be updated so module may add some specific
 * message to the end of the status text. Returned text should either be
 * allocated or %NULL.
 *
 * The @init callback is done once on module loading. It it exists then
 * it should return %TRUE after successful initialization.
 *
 * The @finalize is done on the file manager termination. It should free
 * any resources allocated in @init callback.
 *
 * The key for module of this type is ignored in this implementation.
 */
typedef struct {
    gboolean (*init)(void);
    void (*finalize)(void);
    char * (*sel_message)(FmFileInfoList *files, gint n_files);
} FmTabPageStatusInit;

extern FmTabPageStatusInit fm_module_init_tab_page_status;

G_END_DECLS

#endif /* __PCMANFM_MODULES_H__ */
