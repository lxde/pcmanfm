#include "file-search-ui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libfm/fm-gtk.h>
#include <math.h>

#include "main-win.h"
#include "pcmanfm.h"

typedef struct
{ 
	FmMainWin * win;

	GtkBuilder * builder;
	GtkWidget * window;

	/* For Search Results */
	GtkWidget * view;
	FmFileSearch * search;

	/* GUI Items */
	GtkWidget * find_button;
	GtkWidget * cancel_button;
	GtkWidget * search_again_button;
	GtkWidget * search_results_frame;
	GtkWidget * search_results_tree_container;
	GtkWidget * find_settings_notebook;

	/* Data Entry Items */
	GtkWidget * file_name_textbox;
	GtkWidget * file_name_case_sensitive_checkbox;
	GtkWidget * file_name_regex_checkbox;
	GtkWidget * path_tree_view;
	GtkWidget * path_tree_view_add_button;
	GtkWidget * path_tree_view_remove_button;
	GtkWidget * path_recursive_checkbox;
	GtkWidget * path_show_hidden_checkbox;
	GtkWidget * file_contains_textbox;
	GtkWidget * file_contains_case_sensitive_checkbox;
	GtkWidget * file_contains_regex_checkbox;
	GtkWidget * bigger_checkbox;
	GtkWidget * smaller_checkbox;
	GtkWidget * smaller_spinbutton;
	GtkWidget * bigger_spinbutton;
	goffset max_size;
	goffset min_size;
	GtkWidget * smaller_type;
	GtkWidget * bigger_type;
	GtkWidget * type_selector;
	GtkWidget * calender_checkbox;
	GtkWidget * start_calender;
	GtkWidget * end_calender;
	GtkListStore * path_list_store;
} FileSearchUI;

void add_path(GtkWidget * list, const char * path);

static void on_window_deleted()
{
	pcmanfm_unref();
}

static gchar * mime_types[] = { NULL,
						 "text/plain",
						 "audio/",
						 "video/",
						 "image/" };

static gchar * document_mime_types[] = { "application/pdf",
"application/msword",
"application/vnd.ms-excel",
"application/vnd.ms-powerpoint",
"application/vnd.oasis.opendocument.chart",
"application/vnd.oasis.opendocument.database",
"application/vnd.oasis.opendocument.formula",
"application/vnd.oasis.opendocument.graphics",
"application/vnd.oasis.opendocument.image",
"application/vnd.oasis.opendocument.presentation",
"application/vnd.oasis.opendocument.spreadsheet",
"application/vnd.oasis.opendocument.text",
"application/vnd.oasis.opendocument.text-master",
"application/vnd.oasis.opendocument.text-web",
"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
"application/vnd.openxmlformats-officedocument.presentationml.presentation",
"application/vnd.openxmlformats-officedocument.presentationml.slideshow",
"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
"application/x-abiword",
"application/x-gnumeric",
"application/x-dvi",
NULL};
						 
/* UI Signal Handlers */

void on_find_button(GtkButton * btn, gpointer user_data)
{
	FileSearchUI * ui = (FileSearchUI *)user_data;

	/*show and hide appropriate widgets */
	gtk_widget_hide(ui->find_settings_notebook);
	gtk_widget_hide(ui->find_button);

	char * target = g_strdup(gtk_entry_get_text(ui->file_name_textbox)); /*TODO: free */
	char * target_contains = g_strdup(gtk_entry_get_text(ui->file_contains_textbox)); /*TODO: free */

	if(ui->search != NULL)
	{
		g_object_unref(ui->search);
		ui->search = NULL;
	}

	gboolean iter_has_next;
	char * path;
	GtkTreeIter it;
	iter_has_next = gtk_tree_model_get_iter_first(ui->path_list_store, &it);

	FmPathList * target_folders = fm_path_list_new();

	while(iter_has_next)
	{
		gtk_tree_model_get(ui->path_list_store, &it, 0, &path, -1);

		FmPath * fm_path = fm_path_new(path);
		fm_list_push_tail(target_folders, fm_path);

		iter_has_next = gtk_tree_model_iter_next(ui->path_list_store, &it);
	}

	ui->search = fm_file_search_new(target_folders);

	/* add settings */
	fm_file_search_set_case_sensitive_target(ui->search, gtk_toggle_button_get_active(ui->file_name_case_sensitive_checkbox));
	if(gtk_toggle_button_get_active(ui->file_name_regex_checkbox))
		fm_file_search_set_target_mode(ui->search, FM_FILE_SEARCH_MODE_REGEX);
	else
		fm_file_search_set_target_mode(ui->search, FM_FILE_SEARCH_MODE_EXACT);

	fm_file_search_set_case_sensitive_content(ui->search, gtk_toggle_button_get_active(ui->file_contains_case_sensitive_checkbox));
	if(gtk_toggle_button_get_active(ui->file_contains_regex_checkbox))
		fm_file_search_set_content_mode(ui->search, FM_FILE_SEARCH_MODE_REGEX);
	else
		fm_file_search_set_content_mode(ui->search, FM_FILE_SEARCH_MODE_EXACT);

	fm_file_search_set_recursive(ui->search, gtk_toggle_button_get_active(ui->path_recursive_checkbox));
	fm_file_search_set_show_hidden(ui->search, gtk_toggle_button_get_active(ui->path_show_hidden_checkbox));
	
	/* add rules */

	if(gtk_toggle_button_get_active(ui->bigger_checkbox))
	{
		ui->min_size = (goffset)pow(1024, gtk_combo_box_get_active(ui->bigger_type)) * (goffset)gtk_spin_button_get_value_as_int((GtkSpinButton*)ui->bigger_spinbutton);
		fm_file_search_add_search_func(ui->search, fm_file_search_minimum_size_rule, &ui->min_size);
	}

	if(gtk_toggle_button_get_active(ui->smaller_checkbox))
	{
		ui->max_size = (goffset)pow(1024, gtk_combo_box_get_active(ui->smaller_type)) * (goffset)gtk_spin_button_get_value_as_int((GtkSpinButton*)ui->smaller_spinbutton);
		fm_file_search_add_search_func(ui->search, fm_file_search_maximum_size_rule, &ui->max_size);
	}

	if(target != NULL && g_strcmp0(target, "") != 0)
		fm_file_search_add_search_func(ui->search, fm_file_search_target_rule, target);

	if(gtk_combo_box_get_active(ui->type_selector) >= 1)
	{
		switch(gtk_combo_box_get_active(ui->type_selector))
		{
			case 1:
				fm_file_search_add_search_func(ui->search, fm_file_search_target_type_rule, mime_types[gtk_combo_box_get_active(ui->type_selector)]);
				break;

			case 5:
				fm_file_search_add_search_func(ui->search, fm_file_search_target_type_list_rule, &document_mime_types);
				break;

			default:
				fm_file_search_add_search_func(ui->search, fm_file_search_target_type_generic_rule, mime_types[gtk_combo_box_get_active(ui->type_selector)]);
				break;
		}

	}

	if(target_contains != NULL && g_strcmp0(target_contains, "") != 0)
		fm_file_search_add_search_func(ui->search, fm_file_search_target_contains_rule, target_contains);

	if(gtk_toggle_button_get_active(ui->calender_checkbox))
	{
		FmFileSearchModifiedTimeRuleData * time_data = g_slice_new(FmFileSearchModifiedTimeRuleData);
		gtk_calendar_get_date(ui->start_calender, &time_data->start_y, &time_data->start_m, &time_data->start_d);
		gtk_calendar_get_date(ui->end_calender, &time_data->end_y, &time_data->end_m, &time_data->end_d);

		fm_file_search_add_search_func(ui->search, fm_file_search_modified_time_rule, time_data);
	}

	fm_folder_view_chdir_by_folder(FM_FOLDER_VIEW(ui->view), FM_FOLDER(ui->search));

	fm_file_search_run(ui->search);

	gtk_widget_show(ui->search_results_frame);
	gtk_widget_show(ui->cancel_button);
	gtk_widget_show(ui->search_again_button);
}

void on_search_again_button(GtkButton * btn, gpointer user_data)
{
	FileSearchUI * ui = (FileSearchUI *)user_data;

	gtk_widget_hide(ui->search_results_frame);
	gtk_widget_hide(ui->cancel_button);
	gtk_widget_hide(ui->search_again_button);
	gtk_widget_show(ui->find_settings_notebook);
	gtk_widget_show(ui->find_button);
}

void on_add_path_button(GtkButton * btn, gpointer user_data)
{
	FileSearchUI * ui = (FileSearchUI *)user_data;

    GtkWidget* dlg = gtk_file_chooser_dialog_new(
      _("Select a folder"), GTK_WINDOW(ui->window),
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
      NULL );
      
    gtk_dialog_set_alternative_button_order( GTK_DIALOG( dlg ), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL );
    if( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        char* path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
        add_path(ui->path_list_store, path);
        g_free( path );
    }
    gtk_widget_destroy( dlg );
}

static void on_remove_path_button(GtkButton * btn, gpointer user_data)
{
	FileSearchUI * ui = (FileSearchUI *)user_data;

    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(ui->path_tree_view) );
    if( gtk_tree_selection_get_selected(sel, NULL, &it) )
        gtk_list_store_remove( ui->path_list_store, &it );
}

static gboolean open_folder_func(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    FmMainWin* win = FM_MAIN_WIN(user_data);
    GList* l = folder_infos;
    FmFileInfo* fi = (FmFileInfo*)l->data;
    fm_main_win_chdir(win, fi->path);
    l=l->next;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_add_tab(win, fi->path);
    }
    return TRUE;
}

typedef struct {
	FmFileInfo * info;
	FileSearchUI * ui;
} OnOpenFileData;

static on_open_file(GtkAction* action, OnOpenFileData * data)
{
	gboolean open_file = (0 == strcmp( gtk_action_get_name(action), "OpenAction") );

	if(open_file)
	{
		fm_launch_file_simple(GTK_WINDOW(data->ui->win), NULL, data->info, open_folder_func, data->ui->win);
	}
	else
	{
		FmPath * path = fm_file_info_get_path(data->info);
		FmPath * parent = fm_path_get_parent(path);
		fm_main_win_add_win(data->ui->win, parent);
		pcmanfm_ref();
	}	
}

static const char menu_def[] =
"<ui>"
"<popup name=\"Popup\">"
  "<menuitem name=\"Open\" action=\"OpenAction\" />"
  "<menuitem name=\"OpenFolder\" action=\"OpenFolderAction\" />"
"</popup>"
"</ui>";


static GtkActionEntry menu_actions[] =
{
    { "OpenAction", GTK_STOCK_OPEN, N_("_Open"), NULL, NULL, G_CALLBACK(on_open_file) },
    { "OpenFolderAction", GTK_STOCK_OPEN, N_("Open Containing _Folder"), NULL, NULL, G_CALLBACK(on_open_file) }
};

static void on_file_clicked(FmFolderView * fv, FmFolderViewClickType type, FmFileInfo * fi, FileSearchUI * ui)
{
	switch(type)
	{
		case FM_FV_ACTIVATED:
		if(fi)
		{
			fm_launch_file_simple(GTK_WINDOW(ui->win), NULL, fi, open_folder_func, ui->win);
		}
		break;

		case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            GtkMenu * popup;
            GtkUIManager* menu_mgr;
            GtkActionGroup * action_group = gtk_action_group_new ("PopupActions");
            gtk_action_group_set_translation_domain( action_group, NULL );
            menu_mgr = gtk_ui_manager_new();

			OnOpenFileData * data = g_slice_new(OnOpenFileData);
			data->info = fi;
			data->ui = ui;

            gtk_action_group_add_actions( action_group, menu_actions, G_N_ELEMENTS(menu_actions), data);
            gtk_ui_manager_insert_action_group( menu_mgr, action_group, 0 );
            gtk_ui_manager_add_ui_from_string( menu_mgr, menu_def, -1, NULL );

            popup = gtk_ui_manager_get_widget( menu_mgr, "/Popup" );
            g_object_unref(action_group);

            gtk_menu_popup(popup, NULL, NULL, NULL, data, 3, gtk_get_current_event_time());
        }
		break;

    	case FM_FV_MIDDLE_CLICK:
		if(fi)
		{
			FmPath * path = fm_file_info_get_path(fi);
			FmPath * parent = fm_path_get_parent(path);
			fm_main_win_add_win(ui->win, parent);
			pcmanfm_ref();
		}
		break;
	}
}

gboolean file_search_ui()
{
	FileSearchUI * ui = g_slice_new(FileSearchUI);

	ui->win = fm_main_win_get_last_active();
	ui->builder = gtk_builder_new();
	gtk_builder_add_from_file( ui->builder, PACKAGE_UI_DIR "/filesearch.ui", NULL );

	ui->window = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_dialog_window"));
	ui->find_button = GTK_WIDGET(gtk_builder_get_object(ui->builder, "find_button"));
	ui->cancel_button = GTK_WIDGET(gtk_builder_get_object(ui->builder, "cancel_search_button"));
	ui->search_again_button = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_again_button"));
	ui->search_results_frame = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_results"));
	ui->search_results_tree_container = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_results_container"));
	ui->find_settings_notebook = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_dialog_tabs"));
	
	ui->file_name_textbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "target_inputbox"));
	ui->file_name_case_sensitive_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "case_sensitive_checkbox"));
	ui->file_name_regex_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "regex_checkbox"));
	ui->path_tree_view = GTK_WIDGET(gtk_builder_get_object(ui->builder, "path_tree_view"));
	ui->path_tree_view_add_button = GTK_WIDGET(gtk_builder_get_object(ui->builder, "add_path_button"));
	ui->path_tree_view_remove_button = GTK_WIDGET(gtk_builder_get_object(ui->builder, "remove_path_button"));
	ui->path_recursive_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_recursive_checkbox"));
	ui->path_show_hidden_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "search_hidden_files_checkbox"));
	ui->file_contains_textbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "target_contains_inputbox"));
	ui->file_contains_case_sensitive_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "target_contains_case_sensitive_checkbox"));
	ui->file_contains_regex_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "target_contains_regex_checkbox"));

	ui->bigger_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "bigger_than_cb"));
	ui->smaller_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "smaller_than_cb"));
	ui->bigger_spinbutton = GTK_WIDGET(gtk_builder_get_object(ui->builder, "bigger_spinbutton"));
	ui->smaller_spinbutton = GTK_WIDGET(gtk_builder_get_object(ui->builder, "smaller_spinbutton"));
	ui->smaller_type = GTK_WIDGET(gtk_builder_get_object(ui->builder, "smaller_than_type"));
	ui->bigger_type = GTK_WIDGET(gtk_builder_get_object(ui->builder, "bigger_than_type"));

	ui->type_selector = GTK_WIDGET(gtk_builder_get_object(ui->builder, "type_selection"));

	ui->calender_checkbox = GTK_WIDGET(gtk_builder_get_object(ui->builder, "calender_checkbox"));;
	ui->start_calender = GTK_WIDGET(gtk_builder_get_object(ui->builder, "start_calender"));;
	ui->end_calender = GTK_WIDGET(gtk_builder_get_object(ui->builder, "end_calender"));;

	gtk_widget_hide(ui->cancel_button);
	gtk_widget_hide(ui->search_again_button);
	gtk_widget_hide(ui->search_results_frame);

	gtk_builder_connect_signals(ui->builder, NULL);

	/* create path list store */
	ui->path_list_store = gtk_list_store_new(1, G_TYPE_STRING);

	/* connect signals */
	g_signal_connect(G_OBJECT(ui->find_button), "clicked", G_CALLBACK(on_find_button), ui);
	g_signal_connect(G_OBJECT(ui->cancel_button), "clicked", G_CALLBACK(on_cancel_button), ui);
	g_signal_connect(G_OBJECT(ui->search_again_button), "clicked", G_CALLBACK(on_search_again_button), ui);
	g_signal_connect(G_OBJECT(ui->path_tree_view_add_button), "clicked", G_CALLBACK(on_add_path_button), ui);
	g_signal_connect(G_OBJECT(ui->path_tree_view_remove_button), "clicked", G_CALLBACK(on_remove_path_button), ui);

	/* create view and pack */
	ui->view = fm_folder_view_new(FM_FV_LIST_VIEW);
	fm_folder_view_set_show_hidden(ui->view, TRUE);
    fm_folder_view_set_selection_mode(FM_FOLDER_VIEW(ui->view), GTK_SELECTION_SINGLE);
	g_signal_connect(ui->view, "clicked", on_file_clicked, ui);
	gtk_container_add(GTK_CONTAINER(ui->search_results_tree_container), ui->view);
	gtk_widget_show(ui->view);

	/* set up path view */
	add_path(ui->path_list_store, g_get_home_dir());
	gtk_tree_view_set_model(ui->path_tree_view, ui->path_list_store);
	gtk_tree_view_set_headers_visible(ui->path_tree_view, FALSE);
	GtkTreeViewColumn * col = gtk_tree_view_column_new_with_attributes(NULL, gtk_cell_renderer_text_new(), "text", 0, NULL );
    gtk_tree_view_append_column(ui->path_tree_view, col);

	g_object_unref(G_OBJECT(ui->builder));

	gtk_widget_show(ui->window);

	pcmanfm_ref();
	g_signal_connect(G_OBJECT(ui->window), "delete-event", G_CALLBACK(on_window_deleted), NULL);

	return TRUE;
}

void add_path( GtkWidget * list_store, const char * path )
{
    GtkTreeIter it;
    gtk_list_store_append( list_store, &it );
    gtk_list_store_set( list_store, &it, 0, path, -1 );
}


