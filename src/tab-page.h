//      fm-tab-page.h
//
//      Copyright 2011 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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


#ifndef __FM_TAB_PAGE_H__
#define __FM_TAB_PAGE_H__

#include <gtk/gtk.h>
#include <libfm/fm.h>

G_BEGIN_DECLS


#define FM_TYPE_TAB_PAGE                (fm_tab_page_get_type())
#define FM_TAB_PAGE(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj),\
            FM_TYPE_TAB_PAGE, FmTabPage))
#define FM_TAB_PAGE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST((klass),\
            FM_TYPE_TAB_PAGE, FmTabPageClass))
#define FM_IS_TAB_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
            FM_TYPE_TAB_PAGE))
#define FM_IS_TAB_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),\
            FM_TYPE_TAB_PAGE))
#define FM_TAB_PAGE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),\
            FM_TYPE_TAB_PAGE, FmTabPageClass))

typedef enum {
    FM_STATUS_TEXT_NORMAL,
    FM_STATUS_TEXT_SELECTED_FILES,
    FM_STATUS_TEXT_FS_INFO,
    FM_STATUS_TEXT_NUM
}FmStatusTextType;

typedef struct _FmTabPage            FmTabPage;
typedef struct _FmTabPageClass        FmTabPageClass;

struct _FmTabPage
{
    GtkHPaned parent;
    GtkWidget* side_pane;
    GtkWidget* folder_view;
    GtkWidget* tab_label;
    FmNavHistory* nav_history;
    char* status_text[FM_STATUS_TEXT_NUM];
};

struct _FmTabPageClass
{
    GtkHPanedClass parent_class;
    void (*chdir)(FmTabPage* page, FmPath* path);
    void (*open_dir)(FmTabPage* page, guint where, FmPath* path);
    void (*status)(FmTabPage* page, guint type, const char* status_text);
};


GType fm_tab_page_get_type(void);

GtkWidget* fm_tab_page_new(FmPath* path);

void fm_tab_page_chdir(FmTabPage* page, FmPath* path);

void fm_tab_page_set_show_hidden(FmTabPage* page, gboolean show_hidden);

FmPath* fm_tab_page_get_cwd(FmTabPage* page);

GtkWidget* fm_tab_page_get_side_pane(FmTabPage* page);

GtkWidget* fm_tab_page_get_folder_view(FmTabPage* page);

FmFolder* fm_tab_page_get_folder(FmTabPage* page);

FmNavHistory* fm_tab_page_get_history(FmTabPage* page);

void fm_tab_page_reload(FmTabPage* page);

/* go back to next folder */
void fm_tab_page_forward(FmTabPage* page);

/* go back to last folder */
void fm_tab_page_back(FmTabPage* page);

/* jump to specified history item */
void fm_tab_page_history(FmTabPage* page, GList* history_item_link);

/* get window title of this page */
const char* fm_tab_page_get_title(FmTabPage* page);

/* get normal status text */
const char* fm_tab_page_get_status_text(FmTabPage* page, FmStatusTextType type);

G_END_DECLS

#endif /* __FM_TAB_PAGE_H__ */
