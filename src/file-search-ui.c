/* This file is still very rough */

#include "file-search-ui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libfm/fm-gtk.h>
#include <math.h>

void add_path(GtkWidget * list, const char * path);

GtkBuilder * builder;
GtkWidget * window;

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

/* for size search */
GtkObject * bigger_adjustment;
GtkObject * smaller_adjustment;
GtkWidget * bigger_checkbox;
GtkWidget * smaller_checkbox;

GtkWidget * smaller_spinbutton;
GtkWidget * bigger_spinbutton;
static goffset max_size = 0;
static goffset min_size = 0;
GtkWidget * smaller_type;
GtkWidget * bigger_type;

GtkListStore * path_list_store;

/* search settings */
char * target = NULL;
char * target_contains = NULL;
char * target_type = NULL;
gboolean not_recursive = FALSE;
gboolean show_hidden = FALSE;
gboolean regex_target = FALSE;
gboolean regex_content = FALSE;
gboolean exact_target = FALSE;
gboolean exact_content = FALSE;
gboolean case_sensitive_target = FALSE;
gboolean case_sensitive_content = FALSE;
gint64 minimum_size = -1;
gint64 maximum_size = -1;

/* UI Signal Handlers */

void on_find_button()
{
	/*show and hide appropriate widgets */
	gtk_widget_hide(find_settings_notebook);
	gtk_widget_hide(find_button);
	gtk_widget_show(search_results_frame);
	//gtk_widget_show(cancel_button);
	gtk_widget_show(search_again_button);

	target = g_strdup(gtk_entry_get_text(file_name_textbox)); /*TODO: free */
	target_contains = g_strdup(gtk_entry_get_text(file_contains_textbox)); /*TODO: free */

	if(search != NULL)
		g_object_unref(search);

	gboolean iter_has_next;
	char * path;
	GtkTreeIter it;
	iter_has_next = gtk_tree_model_get_iter_first(path_list_store, &it);

	FmPathList * target_folders = fm_path_list_new();

	while(iter_has_next)
	{
		gtk_tree_model_get(path_list_store, &it, 0, &path, -1);

		FmPath * fm_path = fm_path_new(path);
		fm_list_push_tail(target_folders, fm_path);

		iter_has_next = gtk_tree_model_iter_next(path_list_store, &it);
	}

	search = fm_file_search_new(target_folders);

	/* add settings */
	fm_file_search_set_case_sensitive_target(search, gtk_toggle_button_get_active(file_name_case_sensitive_checkbox));
	if(gtk_toggle_button_get_active(file_name_regex_checkbox))
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else
		fm_file_search_set_target_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	fm_file_search_set_case_sensitive_content(search, gtk_toggle_button_get_active(file_contains_case_sensitive_checkbox));
	if(gtk_toggle_button_get_active(file_contains_regex_checkbox))
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_REGEX);
	else
		fm_file_search_set_content_mode(search, FM_FILE_SEARCH_MODE_EXACT);

	fm_file_search_set_recursive(search, gtk_toggle_button_get_active(path_recursive_checkbox));
	fm_file_search_set_show_hidden(search, gtk_toggle_button_get_active(path_show_hidden_checkbox));
	
	/* add rules */

	if(gtk_toggle_button_get_active(bigger_checkbox))
	{
		min_size = (goffset)pow(1024, gtk_combo_box_get_active(bigger_type)) * (goffset)gtk_spin_button_get_value_as_int((GtkSpinButton*)bigger_spinbutton);
		fm_file_search_add_search_func(search, fm_file_search_minimum_size_rule, &min_size);
	}

	if(gtk_toggle_button_get_active(smaller_checkbox))
	{
		max_size = (goffset)pow(1024, gtk_combo_box_get_active(smaller_type)) * (goffset)gtk_spin_button_get_value_as_int((GtkSpinButton*)smaller_spinbutton);
		fm_file_search_add_search_func(search, fm_file_search_maximum_size_rule, &max_size);
	}

	if(target != NULL && g_strcmp0(target, "") != 0)
		fm_file_search_add_search_func(search, fm_file_search_target_rule, target);


	if(target_contains != NULL && g_strcmp0(target_contains, "") != 0)
		fm_file_search_add_search_func(search, fm_file_search_target_contains_rule, target_contains);

	fm_folder_view_chdir_by_folder(FM_FOLDER_VIEW(view), FM_FOLDER(search));

	fm_file_search_run(search);
}

void on_cancel_button()
{
	gtk_widget_hide(cancel_button);
}

void on_search_again_button()
{
	gtk_widget_hide(search_results_frame);
	gtk_widget_hide(cancel_button);
	gtk_widget_hide(search_again_button);
	gtk_widget_show(find_settings_notebook);
	gtk_widget_show(find_button);

	if(search != NULL)
		g_object_unref(search);
}

void on_add_path_button()
{
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
      _("Select a folder"), GTK_WINDOW(window),
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
      NULL );
      
    gtk_dialog_set_alternative_button_order( GTK_DIALOG( dlg ), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL );
    if( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        char* path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
        add_path(path_list_store, path);
        g_free( path );
    }
    gtk_widget_destroy( dlg );
}

static void on_remove_path_button()
{
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(path_tree_view) );
    if( gtk_tree_selection_get_selected(sel, NULL, &it) )
        gtk_list_store_remove( path_list_store, &it );
}

gboolean file_search_ui()
{
	builder = gtk_builder_new();
	gtk_builder_add_from_file( builder, PACKAGE_UI_DIR "/filesearch.ui", NULL );

	window = GTK_WIDGET(gtk_builder_get_object(builder, "search_dialog_window"));
	find_button = GTK_WIDGET(gtk_builder_get_object(builder, "find_button"));
	cancel_button = GTK_WIDGET(gtk_builder_get_object(builder, "cancel_search_button"));
	search_again_button = GTK_WIDGET(gtk_builder_get_object(builder, "search_again_button"));
	search_results_frame = GTK_WIDGET(gtk_builder_get_object(builder, "search_results"));
	search_results_tree_container = GTK_WIDGET(gtk_builder_get_object(builder, "search_results_container"));
	find_settings_notebook = GTK_WIDGET(gtk_builder_get_object(builder, "search_dialog_tabs"));
	
	file_name_textbox = GTK_WIDGET(gtk_builder_get_object(builder, "target_inputbox"));
	file_name_case_sensitive_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "case_sensitive_checkbox"));
	file_name_regex_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "regex_checkbox"));
	path_tree_view = GTK_WIDGET(gtk_builder_get_object(builder, "path_tree_view"));
	path_tree_view_add_button = GTK_WIDGET(gtk_builder_get_object(builder, "add_path_button"));
	path_tree_view_remove_button = GTK_WIDGET(gtk_builder_get_object(builder, "remove_path_button"));
	path_recursive_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "search_recursive_checkbox"));
	path_show_hidden_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "search_hidden_files_checkbox"));
	file_contains_textbox = GTK_WIDGET(gtk_builder_get_object(builder, "target_contains_inputbox"));
	file_contains_case_sensitive_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "target_contains_case_sensitive_checkbox"));
	file_contains_regex_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "target_contains_regex_checkbox"));

	bigger_adjustment = gtk_builder_get_object(builder, "bigger_adjustment");
	smaller_adjustment = gtk_builder_get_object(builder, "smaller_adjustment");
	bigger_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "bigger_than_cb"));
	smaller_checkbox = GTK_WIDGET(gtk_builder_get_object(builder, "smaller_than_cb"));
	bigger_spinbutton = GTK_WIDGET(gtk_builder_get_object(builder, "bigger_spinbutton"));
	smaller_spinbutton = GTK_WIDGET(gtk_builder_get_object(builder, "smaller_spinbutton"));
	smaller_type = GTK_WIDGET(gtk_builder_get_object(builder, "smaller_than_type"));
	bigger_type = GTK_WIDGET(gtk_builder_get_object(builder, "bigger_than_type"));


	gtk_widget_hide(cancel_button);
	gtk_widget_hide(search_again_button);
	gtk_widget_hide(search_results_frame);

	gtk_builder_connect_signals(builder, NULL);

	/* create path list store */
	path_list_store = gtk_list_store_new(1, G_TYPE_STRING);

	/* connect signals */
	g_signal_connect(G_OBJECT(find_button), "clicked", G_CALLBACK(on_find_button), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(on_cancel_button), NULL);
	g_signal_connect(G_OBJECT(search_again_button), "clicked", G_CALLBACK(on_search_again_button), NULL);
	g_signal_connect(G_OBJECT(path_tree_view_add_button), "clicked", G_CALLBACK(on_add_path_button), NULL);
	g_signal_connect(G_OBJECT(path_tree_view_remove_button), "clicked", G_CALLBACK(on_remove_path_button), NULL);


	/* create view and pack */
	view = fm_folder_view_new(FM_FV_LIST_VIEW);
	fm_folder_view_set_show_hidden(view, TRUE);
	gtk_container_add(GTK_CONTAINER(search_results_tree_container), view);
	gtk_widget_show(view);

	/* set up path view */
	add_path(path_list_store, g_get_home_dir());
	gtk_tree_view_set_model(path_tree_view, path_list_store);
	gtk_tree_view_set_headers_visible(path_tree_view, FALSE);
	GtkTreeViewColumn * col = gtk_tree_view_column_new_with_attributes(NULL, gtk_cell_renderer_text_new(), "text", 0, NULL );
    gtk_tree_view_append_column(path_tree_view, col);

	g_object_unref(G_OBJECT(builder));

	gtk_widget_show(window);

	return TRUE;
}

void add_path( GtkWidget * list_store, const char * path )
{
    GtkTreeIter it;
    gtk_list_store_append( list_store, &it );
    gtk_list_store_set( list_store, &it, 0, path, -1 );
}


