/*
 *      volume-manager.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "volume-manager.h"
#include <fm-gtk.h>
#include "pcmanfm.h"
#include "main-win.h"

static GVolumeMonitor* vol_mon = NULL;

static void on_content_type(GObject* src_obj, GAsyncResult* res, gpointer user_data)
{
    GMount* mount = G_MOUNT(src_obj);
    char** types;
    GFile* gf;
    FmPath* path;
    types = g_mount_guess_content_type_finish(mount, res, NULL);
    if(types)
    {
        char** type;
        for(type=types;*type;++type)
            g_debug("%s", *type);
    }
    g_strfreev(types);
/*
    gf = g_mount_get_root(mount);
    path = fm_path_new_for_gfile(gf);
    fm_main_win_add_win(NULL, path);
    fm_path_unref(path);
    g_object_unref(gf);
*/
}

inline static gboolean automount_volume(GVolume* vol, gboolean silent)
{
    GMount* mount;
    GCancellable* cancellable;

    if(!g_volume_should_automount(vol) || !g_volume_can_mount(vol))
        return FALSE;
    mount = g_volume_get_mount(vol);
    if(!mount) /* not mounted, automount is needed */
    {
        g_debug("try automount");
        if(!fm_mount_volume(NULL, vol))
            return FALSE;
        if(silent)
            return TRUE;
        mount = g_volume_get_mount(vol);
        g_debug("mount = %p", mount);
    }
    if(mount && !silent)
    {
        cancellable = g_cancellable_new();
        g_mount_guess_content_type(mount, TRUE, cancellable, on_content_type, NULL);
    }
    return TRUE;
}

static void on_vol_added(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    automount_volume(vol, FALSE);
    /* TODO: show icons in systray */
}

static void on_vol_removed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    g_debug("vol: %p is removed", vol);
}

static void on_vol_changed(GVolumeMonitor* vm, GVolume* vol, gpointer user_data)
{
    GMount* mount = g_volume_get_mount(vol);
    g_debug("vol: %p is changed, mount = %p", vol, mount);
    if(mount)
        g_object_unref(mount);
}

static gboolean fm_volume_manager_delay_init(gpointer user_data)
{
    GList* vols, *l;
    vol_mon = g_volume_monitor_get();
    if(G_UNLIKELY(!vol_mon))
        return FALSE;

    g_signal_connect(vol_mon, "volume-added", G_CALLBACK(on_vol_added), NULL);
    g_signal_connect(vol_mon, "volume-removed", G_CALLBACK(on_vol_removed), NULL);
    
    /* FIXME: is this needed? */
    g_signal_connect(vol_mon, "volume-changed", G_CALLBACK(on_vol_changed), NULL);

    /* try to automount all volumes */
    vols = g_volume_monitor_get_volumes(vol_mon);
    for(l=vols;l;l=l->next)
    {
        GVolume* vol = G_VOLUME(l->data);
        if(g_volume_should_automount(vol))
            automount_volume(vol, TRUE);
        g_object_unref(vol);
    }
    g_list_free(vols);
    return FALSE;
}

void fm_volume_manager_init()
{
    /* init the volume manager when idle */
    g_idle_add_full(G_PRIORITY_LOW, fm_volume_manager_delay_init, NULL, NULL);
}

void fm_volume_manager_finalize()
{
    if(vol_mon)
    {
        g_signal_handlers_disconnect_by_func(vol_mon, on_vol_added, NULL);
        g_signal_handlers_disconnect_by_func(vol_mon, on_vol_removed, NULL);
        g_signal_handlers_disconnect_by_func(vol_mon, on_vol_changed, NULL);

        g_object_unref(vol_mon);
        vol_mon = NULL;
    }
}
