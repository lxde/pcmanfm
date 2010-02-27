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

static const char desktop_menu_xml[] =
"<popup>"
  "<menu action='CreateNew'>"
    "<menuitem action='NewFolder'/>"
    "<menuitem action='NewBlank'/>"
  "</menu>"
  "<separator/>"
  "<menuitem action='Paste'/>"
  "<separator/>"
  "<menuitem action='SelAll'/>"
  "<menuitem action='InvSel'/>"
  "<separator/>"
  "<menu action='Sort'>"
    "<menuitem action='Asc'/>"
    "<menuitem action='Desc'/>"
    "<separator/>"
    "<menuitem action='ByName'/>"
    "<menuitem action='ByMTime'/>"
    "<menuitem action='BySize'/>"
    "<menuitem action='ByType'/>"
  "</menu>"
  "<separator/>"
  "<menuitem action='Prop'/>"
"</popup>";

static const GtkActionEntry desktop_actions[]=
{
    {"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, G_CALLBACK(on_paste)},
    {"SelAll", GTK_STOCK_SELECT_ALL, NULL, NULL, NULL, G_CALLBACK(on_select_all)},
    {"InvSel", NULL, N_("_Invert Selection"), "<Ctrl>I", NULL, G_CALLBACK(on_invert_select)},
    {"Sort", NULL, N_("_Sort Files"), NULL, NULL, NULL},
    {"CreateNew", GTK_STOCK_NEW, NULL, NULL, NULL, NULL},
    {"NewFolder", "folder", N_("Folder"), NULL, NULL, G_CALLBACK(on_create_new)},
    {"NewBlank", "text-x-generic", N_("Blank File"), NULL, NULL, G_CALLBACK(on_create_new)},
    {"Prop", GTK_STOCK_PROPERTIES, N_("Desktop Preferences"), "<Alt>Return", NULL, G_CALLBACK(fm_desktop_preference)}
};

static const GtkRadioActionEntry desktop_sort_type_actions[]=
{
    {"Asc", GTK_STOCK_SORT_ASCENDING, NULL, NULL, NULL, GTK_SORT_ASCENDING},
    {"Desc", GTK_STOCK_SORT_DESCENDING, NULL, NULL, NULL, GTK_SORT_DESCENDING},
};

static const GtkRadioActionEntry desktop_sort_by_actions[]=
{
    {"ByName", NULL, N_("By _Name"), NULL, NULL, COL_FILE_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), NULL, NULL, COL_FILE_MTIME},
    {"BySize", NULL, N_("By _Size"), NULL, NULL, COL_FILE_SIZE},
    {"ByType", NULL, N_("By File _Type"), NULL, NULL, COL_FILE_DESC}
};

static const char folder_menu_xml[]=
"<popup>"
  "<placeholder name='ph1'>"
    "<menuitem action='NewTab'/>"
    "<menuitem action='NewWin'/>"
    "<menuitem action='Term'/>"
    /* "<menuitem action='Search'/>" */
  "</placeholder>"
"</popup>";

/* Action entries for pupup menus */
static const GtkActionEntry folder_menu_actions[]=
{
    {"NewTab", GTK_STOCK_NEW, N_("Open in New Tab"), NULL, NULL, G_CALLBACK(on_open_in_new_tab)},
    {"NewWin", GTK_STOCK_NEW, N_("Open in New Window"), NULL, NULL, G_CALLBACK(on_open_in_new_win)},
    {"Search", GTK_STOCK_FIND, NULL, NULL, NULL, NULL},
    {"Term", "utilities-terminal", N_("Open in _Terminal"), NULL, NULL, G_CALLBACK(on_open_folder_in_terminal)}
};

