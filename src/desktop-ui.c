/*
 *      desktop-ui.c
 *
 *      Copyright 2010 PCMan <pcman.tw@gmail.com>
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

/* this file is included by desktop.c */

/* FmFolderView popup extension */
static const char desktop_menu_xml[]=
"<popup>"
  "<placeholder name='CustomCommonOps'>"
    "<menuitem action='DeskPref'/>"
  "</placeholder>"
"</popup>";

static const GtkActionEntry desktop_actions[]=
{
    {"DeskPref", GTK_STOCK_PROPERTIES, N_("Desktop Preferences"), "", NULL, G_CALLBACK(fm_desktop_preference)}
};

/* FmFileMenu extension for single folder */
static const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<menuitem action='Term'/>"
    /* "<menuitem action='Search'/>" */
  "</placeholder>"
"</popup>";

/* Additional action entries for popup menus - check mnemonics in FmFileMenu */
static const GtkActionEntry folder_menu_actions[]=
{
    {"NewTab", GTK_STOCK_NEW, N_("Open in New Ta_b"), NULL, NULL, G_CALLBACK(on_open_in_new_tab)},
    {"NewWin", GTK_STOCK_NEW, N_("Open in New Win_dow"), NULL, NULL, G_CALLBACK(on_open_in_new_win)},
    {"Search", GTK_STOCK_FIND, NULL, NULL, NULL, NULL},
    {"Term", "utilities-terminal", N_("Open in Termina_l"), NULL, NULL, G_CALLBACK(on_open_folder_in_terminal)}
};

#if FM_CHECK_VERSION(1, 2, 0)
static const char extra_item_menu_xml[]=
"<popup>"
  "<placeholder name='ph2'>"
    "<separator/>"
    "<menuitem action='Disable'/>"
  "</placeholder>"
"</popup>";

static const GtkActionEntry extra_item_menu_actions[]=
{
    {"Disable", NULL, N_("_Remove from Desktop"), NULL, NULL, G_CALLBACK(on_disable)}
};
#endif

/* xml definition for desktop item placement */
static const char desktop_icon_menu_xml[]=
"<popup>"
  "<placeholder name='ph2'>"
    "<separator/>"
    "<menuitem action='Fix'/>"
    "<menuitem action='Snap'/>"
  "</placeholder>"
"</popup>";

/* action entries for desktop item placement */
static GtkToggleActionEntry desktop_icon_toggle_actions[]=
{
    {"Fix", NULL, N_("Stic_k to Current Position"), NULL, NULL, G_CALLBACK(on_fix_pos), FALSE}
};

static const GtkActionEntry desktop_icon_actions[]=
{
    {"Snap", NULL, N_("Snap to _Grid"), NULL, NULL, G_CALLBACK(on_snap_to_grid)}
};
