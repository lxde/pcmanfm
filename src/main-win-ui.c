/*
 *      main-win-ui.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

/* this file is included by main-win.c */

static const char main_menu_xml[] =
"<menubar>"
  "<menu action='FileMenu'>"
    "<menuitem action='New'/>"
    "<menuitem action='NewTab'/>"
    "<separator/>"
    "<menu action='CreateNew'>"
      "<menuitem action='NewFolder'/>"
      "<menuitem action='NewBlank'/>"
    "</menu>"
    "<separator/>"
    "<menuitem action='CloseTab'/>"
    "<menuitem action='Close'/>"
  "</menu>"
  "<menu action='EditMenu'>"
    "<menuitem action='Cut'/>"
    "<menuitem action='Copy'/>"
    "<menuitem action='Paste'/>"
    "<menuitem action='Del'/>"
    "<separator/>"
    "<menuitem action='Rename'/>"
    /* TODO: implement symlink creation.
    "<menuitem action='Link'/>"
    */
    "<menuitem action='MoveTo'/>"
    "<menuitem action='CopyTo'/>"
    "<separator/>"
    "<menuitem action='SelAll'/>"
    "<menuitem action='InvSel'/>"
    "<separator/>"
    "<menuitem action='Pref'/>"
  "</menu>"
  "<menu action='GoMenu'>"
    "<menuitem action='Prev'/>"
    "<menuitem action='Next'/>"
    "<menuitem action='Up'/>"
    "<separator/>"
    "<menuitem action='Home'/>"
    "<menuitem action='Desktop'/>"
    "<menuitem action='Computer'/>"
    "<menuitem action='Trash'/>"
    "<menuitem action='Network'/>"
    "<menuitem action='Apps'/>"
  "</menu>"
  "<menu action='BookmarksMenu'>"
    "<menuitem action='AddBookmark'/>"
  "</menu>"
  "<menu action='ViewMenu'>"
    "<menuitem action='Reload'/>"
    "<menuitem action='ShowHidden'/>"
    "<menu action='SidePane'>"
      "<menuitem action='ShowSidePane' />"
      "<separator/>"
      "<menuitem action='Places' />"
      "<menuitem action='DirTree' />"
    "</menu>"
    /* "<menuitem action='ShowStatus'/>" */
    /* "<menuitem action='TwinPane'/>" */
    "<menuitem action='Fullscreen' />"
    "<separator/>"
    "<menuitem action='IconView'/>"
    "<menuitem action='ThumbnailView'/>"
    "<menuitem action='CompactView'/>"
    "<menuitem action='ListView'/>"
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
  "</menu>"
  "<menu action='ToolMenu'>"
    "<menuitem action='Term'/>"
    "<menuitem action='AsRoot'/>"
#if FM_CHECK_VERSION(1, 0, 2)
    "<menuitem action='Search'/>"
#endif
  "</menu>"
  "<menu action='HelpMenu'>"
    "<menuitem action='About'/>"
    "<menuitem action='KeyNavList'/>"
  "</menu>"
"</menubar>"
"<toolbar>"
    "<toolitem action='NewTab'/>"
    "<toolitem action='Next'/>"
    "<toolitem action='Up'/>"
    "<toolitem action='Home'/>"
    "<toolitem action='Go'/>"
"</toolbar>"
"<accelerator action='Location'/>"
"<accelerator action='Location2'/>"
"<accelerator action='Prev2'/>"
"<accelerator action='Next2'/>"
"<accelerator action='Reload2'/>";

/* For actions that are bounced to FmFolderView - check accels for accordance */
static GtkActionEntry main_win_actions[]=
{
    {"FileMenu", NULL, N_("_File"), NULL, NULL, NULL},
        {"New", GTK_STOCK_NEW, N_("_New Window"), "<Ctrl>N", NULL, G_CALLBACK(on_new_win)},
        {"NewTab", "tab-new", N_("New T_ab"), "<Ctrl>T", N_("Create new tab for this folder"), G_CALLBACK(on_new_tab)},
        {"CreateNew", GTK_STOCK_ADD, N_("C_reate New..."), "", NULL, NULL},
        {"CloseTab", GTK_STOCK_CLOSE, N_("_Close Tab"), "<Ctrl>W", NULL, G_CALLBACK(on_close_tab)},
        {"Close", GTK_STOCK_QUIT, N_("Close _Window"), "<Ctrl>Q", NULL, G_CALLBACK(on_close_win)},
    {"EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL},
        {"Cut", GTK_STOCK_CUT, NULL, NULL, NULL, G_CALLBACK(bounce_action)},
        {"Copy", GTK_STOCK_COPY, NULL, NULL, NULL, G_CALLBACK(bounce_action)},
        {"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, G_CALLBACK(bounce_action)},
        {"Del", GTK_STOCK_DELETE, NULL, "Delete", NULL, G_CALLBACK(bounce_action)},
        {"Rename", NULL, N_("_Rename"), "F2", NULL, G_CALLBACK(on_rename)},
        {"Link", NULL, N_("Create Symlin_k"), NULL, NULL, NULL},
        {"MoveTo", NULL, N_("_Move To..."), NULL, NULL, G_CALLBACK(on_move_to)},
        {"CopyTo", NULL, N_("C_opy To..."), NULL, NULL, G_CALLBACK(on_copy_to)},
        {"SelAll", GTK_STOCK_SELECT_ALL, NULL, "<Ctrl>A", NULL, G_CALLBACK(bounce_action)},
        {"InvSel", NULL, N_("_Invert Selection"), "<Ctrl>I", NULL, G_CALLBACK(bounce_action)},
        {"Pref", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, NULL, G_CALLBACK(on_preference)},
    {"ViewMenu", NULL, N_("_View"), NULL, NULL, NULL},
        {"Reload", NULL, N_("_Reload Folder"), "F5", N_("Reload current folder"), G_CALLBACK(on_reload)},
        {"SidePane", NULL, N_("Side _Pane"), NULL, NULL, NULL},
        /* other see below: 'ShowHidden' 'ShowStatus' 'Fullscreen' 'IconView'... */
        {"Sort", NULL, N_("S_ort Files"), NULL, NULL, NULL},
    {"HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL},
        {"About", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK(on_about)},
        {"KeyNavList", GTK_STOCK_INFO, N_("_Keyboard navigation"), NULL, NULL, G_CALLBACK(on_key_nav_list)},
    {"GoMenu", NULL, N_("_Go"), NULL, NULL, NULL},
        {"Prev", GTK_STOCK_GO_BACK, N_("_Previous Folder"), "<Alt>Left", N_("Previous Folder"), G_CALLBACK(on_go_back)},
        {"Next", GTK_STOCK_GO_FORWARD, N_("_Next Folder"), "<Alt>Right", N_("Next Folder"), G_CALLBACK(on_go_forward)},
        {"Up", GTK_STOCK_GO_UP, N_("Pa_rent Folder"), "<Alt>Up", N_("Go to parent Folder"), G_CALLBACK(on_go_up)},
        {"Home", "user-home", N_("H_ome Folder"), "<Alt>Home", N_("Home Folder"), G_CALLBACK(on_go_home)},
        {"Desktop", "user-desktop", N_("_Desktop"), NULL, N_("Desktop Folder"), G_CALLBACK(on_go_desktop)},
        {"Computer", "computer", N_("_My Computer"), NULL, NULL, G_CALLBACK(on_go_computer)},
        {"Trash", "user-trash", N_("_Trash Can"), NULL, NULL, G_CALLBACK(on_go_trash)},
        {"Network", GTK_STOCK_NETWORK, N_("Net_work Drives"), NULL, NULL, G_CALLBACK(on_go_network)},
        {"Apps", "system-software-install", N_("_Applications"), NULL, N_("Installed Applications"), G_CALLBACK(on_go_apps)},
        {"Go", GTK_STOCK_JUMP_TO, NULL, NULL, N_("Open entered folder"), G_CALLBACK(on_go)},
    {"BookmarksMenu", NULL, N_("_Bookmarks"), NULL, NULL, NULL},
        {"AddBookmark", GTK_STOCK_ADD, N_("_Add To Bookmarks"), "<Ctrl>D", NULL, G_CALLBACK(on_add_bookmark)},
    {"ToolMenu", NULL, N_("Tool_s"), NULL, NULL, NULL},
        {"Term", "utilities-terminal", N_("Open Current Folder in _Terminal"), "F4", NULL, G_CALLBACK(on_open_in_terminal)},
        {"AsRoot", GTK_STOCK_DIALOG_AUTHENTICATION, N_("Open Current Folder as _Root"), NULL, NULL, G_CALLBACK(on_open_as_root)},
#if FM_CHECK_VERSION(1, 0, 2)
        {"Search", GTK_STOCK_FIND, N_("Fin_d Files"), "<Ctrl><Shift>F", NULL, G_CALLBACK(on_search)},
#endif
    /* for accelerators */
    {"Location", NULL, NULL, "<Alt>d", NULL, G_CALLBACK(on_location)},
    {"Location2", NULL, NULL, "<Ctrl>L", NULL, G_CALLBACK(on_location)},
    {"Prev2", NULL, NULL, "XF86Back", NULL, G_CALLBACK(on_go_back)},
    {"Next2", NULL, NULL, "XF86Forward", NULL, G_CALLBACK(on_go_forward)},
    {"Reload2", NULL, NULL, "<Ctrl>R", NULL, G_CALLBACK(on_reload)},
    /* for popup menu - these entries exist in FmFolderView popup still */
    {"NewFolder", "folder", N_("Folder"), "<Ctrl><Shift>N", NULL, G_CALLBACK(bounce_action)},
    {"NewBlank", NULL, N_("Empty File"), "<Ctrl><Alt>N", NULL, G_CALLBACK(bounce_action)}
};

/* main_win_toggle_actions+main_win_mode_actions - see 'ViewMenu' for mnemonics */
static GtkToggleActionEntry main_win_toggle_actions[]=
{
    {"ShowHidden", NULL, N_("Show Hidde_n"), "<Ctrl>H", NULL, G_CALLBACK(on_show_hidden), FALSE},
    {"ShowSidePane", NULL, N_("Sho_w Side Pane"), "F9", NULL, G_CALLBACK(on_show_side_pane), TRUE},
    {"ShowStatus", NULL, N_("Show Status B_ar"), "<Ctrl>B", NULL, NULL, TRUE},
    {"TwinPane", NULL, N_("T_win Pane Mode"), "F3", NULL, NULL, TRUE},
    {"Fullscreen", NULL, N_("Fullscreen _Mode"), "F11", NULL, G_CALLBACK(on_fullscreen), FALSE}
};

static GtkRadioActionEntry main_win_mode_actions[]=
{
    {"IconView", NULL, N_("_Icon View"), "<Ctrl>1", NULL, FM_FV_ICON_VIEW},
    {"ThumbnailView", NULL, N_("_Thumbnail View"), "<Ctrl>2", NULL, FM_FV_THUMBNAIL_VIEW},
    {"CompactView", NULL, N_("_Compact View"), "<Ctrl>3", NULL, FM_FV_COMPACT_VIEW},
    {"ListView", NULL, N_("Detailed _List View"), "<Ctrl>4", NULL, FM_FV_LIST_VIEW},
};

static GtkRadioActionEntry main_win_sort_type_actions[]=
{
    {"Asc", GTK_STOCK_SORT_ASCENDING, NULL, NULL, NULL, GTK_SORT_ASCENDING},
    {"Desc", GTK_STOCK_SORT_DESCENDING, NULL, NULL, NULL, GTK_SORT_DESCENDING},
};

static GtkRadioActionEntry main_win_sort_by_actions[]=
{
    {"ByName", NULL, N_("By _Name"), "<Alt><Ctrl>1", NULL, COL_FILE_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), "<Alt><Ctrl>2", NULL, COL_FILE_MTIME},
    {"BySize", NULL, N_("By _Size"), "<Alt><Ctrl>3", NULL, COL_FILE_SIZE},
    {"ByType", NULL, N_("By File _Type"), "<Alt><Ctrl>4", NULL, COL_FILE_DESC}
};

static GtkRadioActionEntry main_win_side_bar_mode_actions[]=
{
    {"Places", NULL, N_("Places"), "<Ctrl>6", NULL, FM_SP_PLACES},
    {"DirTree", NULL, N_("Directory Tree"), "<Ctrl>7", NULL, FM_SP_DIR_TREE},
    {"Remote", NULL, N_("Remote"), "<Ctrl>8", NULL, FM_SP_REMOTE},
};

