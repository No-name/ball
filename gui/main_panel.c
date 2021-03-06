#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "ball.h"

#include "gui_client.h"


extern struct list_head ball_peer_info_set;

GType peer_info_type[PEER_INFO_MAX] = 
{
	[PEER_INFO_NAME] = G_TYPE_STRING,
};

void ball_present_undeliver_message(struct peer_info * peer)
{
	struct message_packet * msg;

	while (!list_empty(&peer->msg_unshow))
	{
		msg = list_first_entry(&peer->msg_unshow, struct message_packet, next);
		list_del(&msg->next);

		ball_chart_panel_present_message(peer->chart_panel, msg);

		free(msg);
	}
}

int ball_peer_info_check_exist(char * name, int name_len)
{
	struct peer_info * peer;

	list_for_each_entry(peer, &ball_peer_info_set, next)
	{
		if (strlen(peer->name) != name_len)
			continue;

		if (!strncmp(peer->name, name, name_len))
			return TRUE;
	}

	return FALSE;
}

void ball_peer_info_set_chart_panel(char * name, BallChartPanel * chart_panel)
{
	struct peer_info * peer;

	list_for_each_entry(peer, &ball_peer_info_set, next)
	{
		if (!strcmp(peer->name, name))
		{
			peer->chart_panel = chart_panel;

			ball_present_undeliver_message(peer);
			return;
		}
	}
}

void ball_peer_info_unset_chart_panel(char * name)
{
	struct peer_info * peer;
	list_for_each_entry(peer, &ball_peer_info_set, next)
	{
		if (!strcmp(peer->name, name))
		{
			peer->chart_panel = NULL;
			return;
		}
	}
}

void ball_main_panel_swap_conn_status(BallMainPanel * main_panel)
{
	if (main_panel->conn_status)
	{
		gtk_widget_hide(main_panel->label_connect);
		gtk_widget_hide(main_panel->image_connect);

		gtk_widget_show(main_panel->label_disconnect);
		gtk_widget_show(main_panel->image_disconnect);

		gtk_button_set_label(GTK_BUTTON(main_panel->button_conn), "Connect");

		main_panel->conn_status = FALSE;
	}
	else
	{
		gtk_widget_hide(main_panel->label_disconnect);
		gtk_widget_hide(main_panel->image_disconnect);

		gtk_widget_show(main_panel->label_connect);
		gtk_widget_show(main_panel->image_connect);

		gtk_button_set_label(GTK_BUTTON(main_panel->button_conn), "Disonnect");
		main_panel->conn_status = TRUE;
	}
}

extern int ball_transfor_active_flag;

void ball_main_panel_responed_conn_button(GtkWidget * button, BallMainPanel * main_panel)
{
	if (main_panel->conn_status && ball_transfor_active_flag != BALL_TRANSFOR_INACTIVE)
	{
		ball_transfor_active_flag = BALL_TRANSFOR_INACTIVE;
	}
	else if (main_panel->conn_status == FALSE && ball_transfor_active_flag == BALL_TRANSFOR_INACTIVE)
	{
		ball_start_transfor_routine();
	}
	else
	{
		return;
	}

	ball_main_panel_swap_conn_status(main_panel);
}

gboolean ball_check_main_panel_close(GtkWidget * window, GdkEvent * event, gpointer user_data)
{
	GtkWidget * dialog;
	GtkWidget * content;
	GtkWidget * label;

	int ret;

	dialog = gtk_dialog_new_with_buttons(NULL,
			GTK_WINDOW(window), 
			GTK_DIALOG_MODAL,
			GTK_STOCK_OK,
			GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT,
			NULL);

	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	label = gtk_label_new("Do you want close the program");
	gtk_widget_set_margin_top(label, 20);
	gtk_widget_set_margin_bottom(label, 30);
	gtk_widget_set_margin_left(label, 20);
	gtk_widget_set_margin_right(label, 20);

	gtk_container_add(GTK_CONTAINER(content), label);

	gtk_widget_show_all(content);

	ret =  gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	switch (ret)
	{
		case GTK_RESPONSE_REJECT:
			return TRUE;
		default:
			return FALSE;
	}
}

void ball_main_panel_responed_row_activated(GtkTreeView * tree, GtkTreePath * path, GtkTreeViewColumn * column, BallMainPanel * main_panel)
{
	GtkWidget * chart_panel;
	GtkTreeIter iter;
	gchar * name;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(main_panel->peer_list_store), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(main_panel->peer_list_store), &iter, PEER_INFO_NAME, &name, -1);

	chart_panel = ball_chart_panel_new();
	gtk_window_set_title(GTK_WINDOW(chart_panel), name);

	BALL_CHART_PANEL(chart_panel)->name = strdup(name);
	BALL_CHART_PANEL(chart_panel)->name_len = strlen(name);

	ball_peer_info_set_chart_panel(name, BALL_CHART_PANEL(chart_panel));

	gtk_widget_show_all(chart_panel);
	g_free(name);
}

void ball_main_panel_init(BallMainPanel * main_panel, BallMainPanelClass * main_panel_class)
{
	GtkTreeIter iter;
	GtkWidget * hbox_status;
	GtkWidget * vbox_main;
	GtkWidget * window = GTK_WIDGET(main_panel);

	gtk_window_set_title(GTK_WINDOW(window), "Nice Day");
	gtk_widget_set_size_request(window, 350, 800);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	g_signal_connect(window, "delete-event", G_CALLBACK(ball_check_main_panel_close), NULL);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	main_panel->peer_list_store = gtk_list_store_new(PEER_INFO_MAX, peer_info_type[PEER_INFO_NAME]);

	main_panel->peer_list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(main_panel->peer_list_store));
	g_signal_connect(main_panel->peer_list_view, "row-activated", G_CALLBACK(ball_main_panel_responed_row_activated), main_panel);

	main_panel->name_renderer = gtk_cell_renderer_text_new();

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(main_panel->peer_list_view), -1, "Peer Name", main_panel->name_renderer, "text", PEER_INFO_NAME, NULL);

	main_panel->peer_scrolled_view = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(main_panel->peer_scrolled_view), main_panel->peer_list_view);

	main_panel->button_conn = gtk_button_new_with_label("");
	g_signal_connect(main_panel->button_conn, "clicked", G_CALLBACK(ball_main_panel_responed_conn_button), main_panel);

	main_panel->label_conn = gtk_label_new("Connection status:");
	main_panel->label_connect = gtk_label_new("On");
	main_panel->label_disconnect = gtk_label_new("Off");

	main_panel->image_connect = gtk_image_new_from_stock(GTK_STOCK_CONNECT, GTK_ICON_SIZE_SMALL_TOOLBAR);
	main_panel->image_disconnect = gtk_image_new_from_stock(GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_SMALL_TOOLBAR);

	hbox_status = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->label_conn, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->label_connect, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->label_disconnect, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->image_connect, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->image_disconnect, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(hbox_status), main_panel->button_conn, TRUE, TRUE, 2);

	vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox_main), main_panel->peer_scrolled_view, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(vbox_main), hbox_status, FALSE, FALSE, 2);

	gtk_container_add(GTK_CONTAINER(window), vbox_main);

	main_panel->conn_status == FALSE;
}

GType ball_main_panel_get_type()
{
	static GType type;

	if (!type)
	{
		GTypeInfo typeInfo = {
			sizeof(BallMainPanelClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(BallMainPanel),
			0,
			(GInstanceInitFunc)ball_main_panel_init,
			NULL,
		};

		type = g_type_register_static(GTK_TYPE_WINDOW, "main_panel", &typeInfo, 0);
	}

	return type;
}

GtkWidget * ball_main_panel_new()
{
	return GTK_WIDGET(g_object_new(BALL_TYPE_MAIN_PANEL, "type", GTK_WINDOW_TOPLEVEL, NULL));
}

void ball_add_peer_info(BallMainPanel * main_panel, struct peer_info * peer_info)
{
	GtkTreeIter iter;

	list_add_tail(&peer_info->next, &ball_peer_info_set);

	gtk_list_store_insert_with_values(main_panel->peer_list_store, &iter, -1, PEER_INFO_NAME, peer_info->name, -1);
}

void ball_test_initial_peer_members(BallMainPanel * main_panel)
{
	struct peer_info * peer_info;
	char ** name;
	char * members[] = {
		"Hokin",
		"Obama",
		"Meilin",
		"Pager",
		NULL
	};

	for (name = members; *name; name++)
	{
		peer_info = malloc(sizeof(struct peer_info));
		memset(peer_info, 0, sizeof(struct peer_info));

		INIT_LIST_HEAD(&peer_info->msg_unshow);

		strcpy(peer_info->name, *name);
		ball_add_peer_info(main_panel, peer_info);
	}
}
