/*
 *      main-win-ui.c
 *
 *      Copyright 2009 - 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
    "<menuitem action='Prop'/>"
    "<separator/>"
    "<menuitem action='CloseTab'/>"
    "<menuitem action='Close'/>"
  "</menu>"
  "<menu action='EditMenu'>"
    "<menuitem action='Open'/>"
    "<separator/>"
    "<menuitem action='Cut'/>"
    "<menuitem action='Copy'/>"
    "<menuitem action='Paste'/>"
    "<menuitem action='ToTrash'/>"
    "<menuitem action='Del'/>"
    "<menuitem action='CopyPath'/>"
    "<separator/>"
    "<menuitem action='FileProp'/>"
    "<separator/>"
    "<menuitem action='Rename'/>"
    "<menuitem action='Link'/>"
    /* TODO: implement "Create a duplicate" action
    "<menuitem action='Duplicate'/>" */
    "<menuitem action='MoveTo'/>"
    "<menuitem action='CopyTo'/>"
    "<separator/>"
    "<menuitem action='SelAll'/>"
    "<menuitem action='InvSel'/>"
    "<separator/>"
    "<menuitem action='Pref'/>"
  "</menu>"
  "<menu action='ViewMenu'>"
    "<menuitem action='Reload'/>"
    "<separator/>"
    "<menuitem action='ShowHidden'/>"
    "<menu action='Sort'>"
      "<menuitem action='Asc'/>"
      "<menuitem action='Desc'/>"
      "<separator/>"
      "<menuitem action='ByName'/>"
      "<menuitem action='ByMTime'/>"
      "<menuitem action='BySize'/>"
      "<menuitem action='ByType'/>"
#if FM_CHECK_VERSION(1, 2, 0)
      "<menuitem action='ByExt'/>"
#endif
#if FM_CHECK_VERSION(1, 0, 2)
      "<separator/>"
#if FM_CHECK_VERSION(1, 2, 0)
      "<menuitem action='MingleDirs'/>"
#endif
      "<menuitem action='SortIgnoreCase'/>"
#endif
    "</menu>"
    "<menu action='FolderView'>"
#if FM_CHECK_VERSION(1, 2, 0)
      "<placeholder name='ViewModes'/>"
#else
      "<menuitem action='IconView'/>"
      "<menuitem action='ThumbnailView'/>"
      "<menuitem action='CompactView'/>"
      "<menuitem action='ListView'/>"
#endif
    "</menu>"
    "<menuitem action='SavePerFolder'/>"
    "<separator/>"
    "<menu action='Toolbar'>"
      "<menuitem action='ShowToolbar'/>"
      "<separator/>"
      "<menuitem action='ToolbarNewWin'/>"
      "<menuitem action='ToolbarNewTab'/>"
      "<menuitem action='ToolbarNav'/>"
      "<menuitem action='ToolbarHome'/>"
    "</menu>"
    "<menu action='PathMode'>"
      "<menuitem action='PathEntry'/>"
      "<menuitem action='PathBar'/>"
    "</menu>"
    "<menu action='SidePane'>"
      "<menuitem action='ShowSidePane' />"
      "<separator/>"
#if FM_CHECK_VERSION(1, 2, 0)
      "<placeholder name='SidePaneModes'/>"
#else
      "<menuitem action='Places' />"
      "<menuitem action='DirTree' />"
#endif
    "</menu>"
    "<menuitem action='ShowStatus'/>"
    "<separator/>"
    "<menuitem action='DualPane'/>"
    "<menuitem action='Fullscreen' />"
    "<separator/>"
    "<menuitem action='SizeBigger'/>"
    "<menuitem action='SizeSmaller'/>"
    "<menuitem action='SizeDefault'/>"
#if FM_CHECK_VERSION(1, 0, 2)
    "<separator/>"
    "<menuitem action='Filter'/>"
#endif
  "</menu>"
  "<menu action='BookmarksMenu'>"
    "<menuitem action='AddBookmark'/>"
  "</menu>"
  "<menu action='GoMenu'>"
    "<menuitem action='Prev'/>"
    "<menuitem action='Next'/>"
    "<menuitem action='Up'/>"
    "<separator/>"
    "<menuitem action='Home'/>"
    "<menuitem action='Desktop'/>"
    "<menuitem action='Trash'/>"
    "<menuitem action='Apps'/>"
    "<menuitem action='Computer'/>"
    "<menuitem action='Network'/>"
    "<separator/>"
    "<menuitem action='Location'/>"
    "<menuitem action='Connect'/>"
  "</menu>"
  "<menu action='ToolMenu'>"
    "<menuitem action='Term'/>"
#if FM_CHECK_VERSION(1, 0, 2)
    "<menuitem action='Search'/>"
#endif
#if FM_CHECK_VERSION(1, 2, 0)
    "<menuitem action='Launch'/>"
#endif
    /* "<menuitem action='AsRoot'/>" */
  "</menu>"
  "<menu action='HelpMenu'>"
    "<menuitem action='About'/>"
    "<menuitem action='KeyNavList'/>"
  "</menu>"
"</menubar>"
"<toolbar>"
    "<toolitem action='New'/>"
    "<toolitem action='NewTab'/>"
#if FM_CHECK_VERSION(1, 2, 0)
    "<toolitem action='Prev'/>"
#endif
    "<toolitem action='Next'/>"
    "<toolitem action='Up'/>"
    "<toolitem action='Home'/>"
    "<toolitem action='Go'/>"
"</toolbar>"
"<accelerator action='Location2'/>"
"<accelerator action='Prev2'/>"
"<accelerator action='Next2'/>"
"<accelerator action='Reload2'/>"
"<accelerator action='SizeBigger2'/>"
"<accelerator action='SizeSmaller2'/>";

/* For actions that are bounced to FmFolderView - check accels for accordance */
static GtkActionEntry main_win_actions[]=
{
    {"FileMenu", NULL, N_("_File"), NULL, NULL, NULL},
        {"New", GTK_STOCK_NEW, N_("_New Window"), "<Ctrl>N", N_("Open new file manager window"), G_CALLBACK(on_new_win)},
        {"NewTab", "tab-new", N_("New T_ab"), "<Ctrl>T", N_("Create new tab for this folder"), G_CALLBACK(on_new_tab)},
        {"CreateNew", GTK_STOCK_ADD, N_("C_reate New..."), "", NULL, NULL},
            {"NewFolder", "folder", N_("Folder"), "<Ctrl><Shift>N", NULL, G_CALLBACK(bounce_action)},
            {"NewBlank", NULL, N_("Empty File"), "<Ctrl><Alt>N", NULL, G_CALLBACK(bounce_action)},
        {"Prop", GTK_STOCK_PROPERTIES, N_("Folder Propertie_s"), NULL, NULL, G_CALLBACK(bounce_action)},
        {"CloseTab", GTK_STOCK_CLOSE, N_("_Close Tab"), "<Ctrl>W", NULL, G_CALLBACK(on_close_tab)},
        {"Close", GTK_STOCK_QUIT, N_("Close _Window"), "<Ctrl>Q", NULL, G_CALLBACK(on_close_win)},
    {"EditMenu", NULL, N_("_Edit"), NULL, NULL, NULL},
        {"Open", GTK_STOCK_OPEN, NULL, "", NULL, G_CALLBACK(on_open)},
        {"Cut", GTK_STOCK_CUT, N_("C_ut"), NULL, NULL, G_CALLBACK(bounce_action)},
        {"Copy", GTK_STOCK_COPY, NULL, NULL, NULL, G_CALLBACK(bounce_action)},
        {"Paste", GTK_STOCK_PASTE, NULL, NULL, NULL, G_CALLBACK(bounce_action)},
        {"ToTrash", GTK_STOCK_DELETE, N_("Move to _Trash"), "", NULL, G_CALLBACK(on_trash)},
        {"Del", GTK_STOCK_REMOVE, NULL, "", NULL, G_CALLBACK(on_del)},
        {"CopyPath", NULL, N_("Copy Pat_h(s)"), NULL, NULL, G_CALLBACK(on_copy_path)},
        {"Rename", NULL, N_("R_ename..."), "F2", NULL, G_CALLBACK(on_rename)},
        {"Duplicate", NULL, N_("D_uplicate..."), "<Ctrl>U", NULL, NULL},
        {"Link", NULL, N_("Create Lin_k..."), NULL, NULL, G_CALLBACK(on_link)},
        {"MoveTo", NULL, N_("_Move to..."), NULL, NULL, G_CALLBACK(on_move_to)},
        {"CopyTo", NULL, N_("Copy to_..."), NULL, NULL, G_CALLBACK(on_copy_to)},
        {"FileProp", GTK_STOCK_PROPERTIES, N_("Propertie_s"), "<Alt>Return", NULL, G_CALLBACK(bounce_action)},
        {"SelAll", GTK_STOCK_SELECT_ALL, NULL, "<Ctrl>A", NULL, G_CALLBACK(bounce_action)},
        {"InvSel", NULL, N_("_Invert Selection"), "<Ctrl>I", NULL, G_CALLBACK(bounce_action)},
        {"Pref", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, NULL, G_CALLBACK(on_preference)},
    {"ViewMenu", NULL, N_("_View"), NULL, NULL, NULL},
        {"Reload", GTK_STOCK_REFRESH, N_("_Reload Folder"), "F5", N_("Reload current folder"), G_CALLBACK(on_reload)},
        {"Toolbar", NULL, N_("Tool_bar"), NULL, NULL, NULL},
        {"PathMode", NULL, N_("Pat_h Bar"), NULL, NULL, NULL},
        {"SidePane", "view-sidetree", N_("Side _Pane"), NULL, NULL, NULL},
        /* other see below: 'ShowHidden' 'ShowStatus' 'Fullscreen' 'IconView'... */
        {"FolderView", "view-choose", N_("Fo_lder View Mode"), NULL, NULL, NULL},
        {"Sort", NULL, N_("S_ort Files"), NULL, NULL, NULL},
        {"SizeBigger", GTK_STOCK_ZOOM_IN, NULL, "<Ctrl>KP_Add", NULL, G_CALLBACK(on_size_increment)},
        {"SizeSmaller", GTK_STOCK_ZOOM_OUT, N_("Zoom O_ut"), "<Ctrl>KP_Subtract", NULL, G_CALLBACK(on_size_decrement)},
        {"SizeDefault", GTK_STOCK_ZOOM_100, NULL, "<Ctrl>0", NULL, G_CALLBACK(on_size_default)},
#if FM_CHECK_VERSION(1, 0, 2)
        {"Filter", "view-filter", N_("Fil_ter..."), "<Ctrl>E", NULL, G_CALLBACK(on_filter)},
#endif
    {"HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL},
        {"About", GTK_STOCK_ABOUT, NULL, NULL, NULL, G_CALLBACK(on_about)},
        {"KeyNavList", GTK_STOCK_INFO, N_("_Keyboard Navigation"), NULL, NULL, G_CALLBACK(on_key_nav_list)},
    {"GoMenu", NULL, N_("_Go"), NULL, NULL, NULL},
        {"Prev", GTK_STOCK_GO_BACK, N_("_Previous Folder"), "<Alt>Left", N_("Return to previous folder in history"), G_CALLBACK(on_go_back)},
        {"Next", GTK_STOCK_GO_FORWARD, N_("_Next Folder"), "<Alt>Right", N_("Go forward to next folder in history"), G_CALLBACK(on_go_forward)},
        {"Up", GTK_STOCK_GO_UP, N_("Pa_rent Folder"), "<Alt>Up", N_("Go to parent Folder"), G_CALLBACK(on_go_up)},
        {"Home", "user-home", N_("H_ome Folder"), "<Alt>Home", N_("Go to home folder"), G_CALLBACK(on_go_home)},
        {"Desktop", "user-desktop", N_("_Desktop"), NULL, N_("Go to desktop folder"), G_CALLBACK(on_go_desktop)},
        {"Trash", "user-trash", N_("_Trash Can"), NULL, N_("Open trash can"), G_CALLBACK(on_go_trash)},
        {"Root", "drive-harddisk", N_("Filesyste_m Root"), NULL, N_("Go fo filesystem root"), NULL},
        {"Apps", "system-software-install", N_("_Applications"), NULL, N_("Go to root of applications menu folder"), G_CALLBACK(on_go_apps)},
        {"Computer", "computer", N_("Dev_ices"), NULL, N_("Go to list of devices connected to the computer"), G_CALLBACK(on_go_computer)},
        {"Network", GTK_STOCK_NETWORK, N_("Net_work"), NULL, N_("Go to list of places on the network"), G_CALLBACK(on_go_network)},
        {"Location", GTK_STOCK_JUMP_TO, N_("_Go to Location..."), "<Ctrl>L", NULL, G_CALLBACK(on_location)},
        {"Connect", NULL, N_("_Connect to Server..."), NULL, N_("Open a window to choose remote folder location"), G_CALLBACK(on_go_connect)},
        {"Go", GTK_STOCK_JUMP_TO, NULL, NULL, N_("Go to the path in the location bar"), G_CALLBACK(on_go)},
    {"BookmarksMenu", NULL, N_("_Bookmarks"), NULL, NULL, NULL},
        {"AddBookmark", GTK_STOCK_ADD, N_("_Add to Bookmarks..."), "<Ctrl>D", N_("Add current folder to bookmarks list"), G_CALLBACK(on_add_bookmark)},
    {"ToolMenu", NULL, N_("Too_ls"), NULL, NULL, NULL},
        {"Term", "utilities-terminal", N_("Open Current Folder in _Terminal"), "F4", NULL, G_CALLBACK(on_open_in_terminal)},
#if FM_CHECK_VERSION(1, 0, 2)
        {"Search", GTK_STOCK_FIND, N_("Fin_d Files..."), "<Ctrl><Shift>F", N_("Open search dialog"), G_CALLBACK(on_search)},
#endif
#if FM_CHECK_VERSION(1, 2, 0)
        {"Launch", GTK_STOCK_EXECUTE, N_("_Run a Command in Current Folder..."), NULL, NULL, G_CALLBACK(on_launch)},
#endif
        /*{"AsRoot", GTK_STOCK_DIALOG_AUTHENTICATION, N_("Open Current Folder as _Root"), NULL, NULL, G_CALLBACK(on_open_as_root)},*/
    /* for accelerators */
    {"Location2", NULL, NULL, "<Alt>d", NULL, G_CALLBACK(on_location)},
    {"Prev2", NULL, NULL, "XF86Back", NULL, G_CALLBACK(on_go_back)},
    {"Next2", NULL, NULL, "XF86Forward", NULL, G_CALLBACK(on_go_forward)},
    {"Reload2", NULL, NULL, "<Ctrl>R", NULL, G_CALLBACK(on_reload)},
    {"SizeBigger2", NULL, NULL, "<Ctrl>equal", NULL, G_CALLBACK(on_size_increment)},
    {"SizeSmaller2", NULL, NULL, "<Ctrl>minus", NULL, G_CALLBACK(on_size_decrement)},
};

/* main_win_toggle_actions+main_win_mode_actions - see 'ViewMenu' for mnemonics */
static GtkToggleActionEntry main_win_toggle_actions[]=
{
#if FM_CHECK_VERSION(1, 2, 0)
    /* Note to translators: "Mingle..." means "Do not put folders before files" but make the translation as short as possible, please! */
    {"MingleDirs", NULL, N_("Mingle _Files and Folders"), NULL, NULL, G_CALLBACK(on_mingle_dirs), FALSE},
#endif
#if FM_CHECK_VERSION(1, 0, 2)
    {"SortIgnoreCase", NULL, N_("_Ignore Name Case"), NULL, NULL, G_CALLBACK(on_sort_ignore_case), TRUE},
#endif
    {"ShowHidden", NULL, N_("Sho_w Hidden"), "<Ctrl>H", NULL, G_CALLBACK(on_show_hidden), FALSE},
    /* Note to translators: this save is meant for folder's settings such as sort */
    {"SavePerFolder", NULL, N_("Preserve This Folder's Settings"), NULL,
            N_("Check to remember view and sort as folder setting rather than global one"),
            G_CALLBACK(on_save_per_folder), FALSE},
    {"ShowToolbar", NULL, N_("_Show Toolbar"), NULL, NULL, G_CALLBACK(on_show_toolbar), TRUE},
    {"ToolbarNewWin", NULL, N_("Show 'New _Window' Button"), NULL, NULL, G_CALLBACK(on_toolbar_new_win), TRUE},
    {"ToolbarNewTab", NULL, N_("Show 'New _Tab' Button"), NULL, NULL, G_CALLBACK(on_toolbar_new_tab), TRUE},
    {"ToolbarNav", NULL, N_("Show _Navigation Buttons"), NULL, NULL, G_CALLBACK(on_toolbar_nav), TRUE},
    {"ToolbarHome", NULL, N_("Show '_Home' Button"), NULL, NULL, G_CALLBACK(on_toolbar_home), TRUE},
    {"ShowSidePane", NULL, N_("Sho_w Side Pane"), "F9", NULL, G_CALLBACK(on_show_side_pane), TRUE},
    {"ShowStatus", NULL, N_("Show Status B_ar"), "<Ctrl>B", NULL, G_CALLBACK(on_show_status), TRUE},
    {"DualPane", NULL, N_("_Dual Pane Mode"), "F3", N_("Show two panels with folder views"), G_CALLBACK(on_dual_pane), FALSE},
    {"Fullscreen", NULL, N_("Fullscreen _Mode"), "F11", NULL, G_CALLBACK(on_fullscreen), FALSE}
};

#if !FM_CHECK_VERSION(1, 2, 0)
static GtkRadioActionEntry main_win_mode_actions[]=
{
    {"IconView", NULL, N_("_Icon View"), "<Ctrl>1", NULL, FM_FV_ICON_VIEW},
    {"CompactView", NULL, N_("_Compact View"), "<Ctrl>2", NULL, FM_FV_COMPACT_VIEW},
    {"ThumbnailView", NULL, N_("_Thumbnail View"), "<Ctrl>3", NULL, FM_FV_THUMBNAIL_VIEW},
    {"ListView", NULL, N_("Detailed _List View"), "<Ctrl>4", NULL, FM_FV_LIST_VIEW},
};
#endif

static GtkRadioActionEntry main_win_sort_type_actions[]=
{
    {"Asc", GTK_STOCK_SORT_ASCENDING, NULL, NULL, NULL, GTK_SORT_ASCENDING},
    {"Desc", GTK_STOCK_SORT_DESCENDING, NULL, NULL, NULL, GTK_SORT_DESCENDING},
};

static GtkRadioActionEntry main_win_sort_by_actions[]=
{
#if FM_CHECK_VERSION(1, 0, 2)
    {"ByName", NULL, N_("By _Name"), "<Alt><Ctrl>1", NULL, FM_FOLDER_MODEL_COL_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), "<Alt><Ctrl>2", NULL, FM_FOLDER_MODEL_COL_MTIME},
    {"BySize", NULL, N_("By _Size"), "<Alt><Ctrl>3", NULL, FM_FOLDER_MODEL_COL_SIZE},
    {"ByType", NULL, N_("By File _Type"), "<Alt><Ctrl>4", NULL, FM_FOLDER_MODEL_COL_DESC},
#if FM_CHECK_VERSION(1, 2, 0)
    {"ByExt", NULL, N_("By _Extension"), "<Alt><Ctrl>5", NULL, FM_FOLDER_MODEL_COL_EXT}
#endif
#else
    {"ByName", NULL, N_("By _Name"), "<Alt><Ctrl>1", NULL, COL_FILE_NAME},
    {"ByMTime", NULL, N_("By _Modification Time"), "<Alt><Ctrl>2", NULL, COL_FILE_MTIME},
    {"BySize", NULL, N_("By _Size"), "<Alt><Ctrl>3", NULL, COL_FILE_SIZE},
    {"ByType", NULL, N_("By File _Type"), "<Alt><Ctrl>4", NULL, COL_FILE_DESC}
#endif
};

#if !FM_CHECK_VERSION(1, 2, 0)
static GtkRadioActionEntry main_win_side_bar_mode_actions[]=
{
    {"Places", NULL, N_("Places"), "<Ctrl>6", NULL, FM_SP_PLACES},
    {"DirTree", NULL, N_("Directory Tree"), "<Ctrl>7", NULL, FM_SP_DIR_TREE},
    {"Remote", NULL, N_("Remote"), "<Ctrl>8", NULL, FM_SP_REMOTE},
};
#endif

static GtkRadioActionEntry main_win_path_bar_mode_actions[]=
{
    {"PathEntry", NULL, N_("_Location"), NULL, NULL, 0},
    {"PathBar", NULL, N_("_Buttons"), NULL, NULL, 1}
};

