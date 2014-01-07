#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "ball.h"

#include "gui_client.h"

extern struct list_head message_need_sended;
extern GMutex mutex_for_message_need_sended;

extern struct list_head message_comming;
extern GMutex mutex_for_message_comming;

gboolean ball_chart_moniter_message_comming(gpointer user_data)
{
	static char * msg_to = "MY HOME";

	static char * msg_sender[] = {
		"Brush",
		"Stevens",
		"Cooke",
		"Riched",
	};

	static int msg_index;

	int i;
	struct message_packet * msg;
	char msg_from[MAX_ACCOUNT_NAME_LEN];
	char msg_content[MAX_MSG_CONTENT_LEN];

	i = msg_index % 4;

	sprintf(msg_from, "%s", msg_sender[i]);
	sprintf(msg_content, "[MSG %d-%d] message hello", getpid(), msg_index);

	msg = malloc(sizeof(struct message_packet));
	msg->type = MSG_TYPE_CHART;
	msg->version = 0x01;

	msg->chart_info.from_len = strlen(msg_from);
	msg->chart_info.from = msg_from;

	msg->chart_info.to_len = strlen(msg_to);
	msg->chart_info.to = msg_to;

	msg->chart_info.msg_len = strlen(msg_content);
	msg->chart_info.msg = msg_content;

	package_message(msg);

	message_parse_head(msg);
	message_parse_body(msg);

	msg_index++;

	g_mutex_lock(&mutex_for_message_comming);

	list_add_tail(&msg->next, &message_comming);

	g_mutex_unlock(&mutex_for_message_comming);

	g_message("Generate a monitor message");

	return TRUE;
}

void ball_chart_panel_send_message(GtkWidget * button, BallChartPanel * chart_panel)
{
	struct message_packet * msg;

	int msg_len;
	GtkTextIter start, end;
	gchar * message_content, * send_from;

	send_from = ball_get_myself_name();

	gtk_text_buffer_get_bounds(chart_panel->msg_send_buffer, &start, &end);
	message_content = gtk_text_buffer_get_text(chart_panel->msg_send_buffer, &start, &end, FALSE);

	msg_len = strlen(message_content);

	if (!msg_len)
		goto out;

	msg = malloc(sizeof(struct message_packet));
	if (!msg)
		goto out;

	msg->type = MSG_TYPE_CHART;
	msg->version = 0x01;

	msg->chart_info.from_len = strlen(send_from);
	msg->chart_info.from = send_from;

	msg->chart_info.to_len = chart_panel->name_len;
	msg->chart_info.to = chart_panel->name;

	msg->chart_info.msg_len = msg_len;
	msg->chart_info.msg = message_content;

	package_message(msg);

	g_mutex_lock(&mutex_for_message_need_sended);
	list_add_tail(&msg->next, &message_need_sended);
	g_mutex_unlock(&mutex_for_message_need_sended);

	gtk_text_buffer_set_text(chart_panel->msg_send_buffer, "", -1);

out:

	g_free(message_content);
}

void ball_chart_panel_clear_message(GtkWidget * button, BallChartPanel * chart_panel)
{
	gtk_text_buffer_set_text(chart_panel->msg_send_buffer, "", -1);
}

void ball_chart_panel_init(BallChartPanel * chart_panel, BallChartPanelClass * chart_panel_class)
{
	GtkWidget * window = GTK_WIDGET(chart_panel);

	gtk_window_set_title(GTK_WINDOW(window), "Chart Message");
	gtk_widget_set_size_request(window, 400, 500);
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	chart_panel->paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_position(GTK_PANED(chart_panel->paned), 300);
	gtk_container_add(GTK_CONTAINER(window), chart_panel->paned);

	chart_panel->msg_tag_table = gtk_text_tag_table_new();
	chart_panel->msg_recv_buffer = gtk_text_buffer_new(chart_panel->msg_tag_table);
	chart_panel->msg_send_buffer = gtk_text_buffer_new(chart_panel->msg_tag_table);

	chart_panel->recv_scroll_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_paned_add1(GTK_PANED(chart_panel->paned), chart_panel->recv_scroll_window);

	chart_panel->msg_recv = gtk_text_view_new_with_buffer(chart_panel->msg_recv_buffer);
	gtk_container_add(GTK_CONTAINER(chart_panel->recv_scroll_window), chart_panel->msg_recv);

	chart_panel->msg_send_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_paned_add2(GTK_PANED(chart_panel->paned), chart_panel->msg_send_view);

	chart_panel->msg_reciver_view = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_view), chart_panel->msg_reciver_view, FALSE, FALSE, 0);

	chart_panel->reciver_label = gtk_label_new("Msg send to :");
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_reciver_view), chart_panel->reciver_label, FALSE, FALSE, 0);

	chart_panel->recivers_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_reciver_view), chart_panel->recivers_combo_box, TRUE, TRUE, 0);

	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(chart_panel->recivers_combo_box), NULL, "Jerry");

	chart_panel->send_scroll_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_view), chart_panel->send_scroll_window, TRUE, TRUE, 0);

	chart_panel->msg_send = gtk_text_view_new_with_buffer(chart_panel->msg_send_buffer);
	gtk_container_add(GTK_CONTAINER(chart_panel->send_scroll_window), chart_panel->msg_send);

	chart_panel->msg_send_button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing(GTK_BOX(chart_panel->msg_send_button_box), 2);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(chart_panel->msg_send_button_box), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_view), chart_panel->msg_send_button_box, FALSE, FALSE, 0);

	chart_panel->send_button = gtk_button_new_with_label("Send");
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_button_box), chart_panel->send_button, FALSE, FALSE, 0);
	g_signal_connect(chart_panel->send_button, "clicked", G_CALLBACK(ball_chart_panel_send_message), chart_panel);

	chart_panel->clear_button = gtk_button_new_with_label("Clear");
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_button_box), chart_panel->clear_button, FALSE, FALSE, 0);
	g_signal_connect(chart_panel->clear_button, "clicked", G_CALLBACK(ball_chart_panel_clear_message), chart_panel);
}

GType ball_chart_panel_get_type()
{
	static GType type;

	if (!type)
	{
		GTypeInfo typeInfo = {
			sizeof(BallChartPanelClass),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			sizeof(BallChartPanel),
			0,
			(GInstanceInitFunc)ball_chart_panel_init,
			NULL,
		};

		type = g_type_register_static(GTK_TYPE_WINDOW, "chart_panel", &typeInfo, 0);
	}

	return type;
}

GtkWidget * ball_chart_panel_new()
{
	return GTK_WIDGET(g_object_new(BALL_TYPE_CHART_PANEL, "type", GTK_WINDOW_TOPLEVEL, NULL));
}

void ball_chart_panel_present_message(BallChartPanel * chart_panel, struct message_packet * msg)
{
	GtkTextIter end;

	gtk_text_buffer_get_end_iter(chart_panel->msg_recv_buffer, &end);

	gtk_text_buffer_insert(chart_panel->msg_recv_buffer, &end, msg->chart_info.from, msg->chart_info.from_len);

	gtk_text_buffer_get_end_iter(chart_panel->msg_recv_buffer, &end);
	gtk_text_buffer_insert(chart_panel->msg_recv_buffer, &end, "\n", -1);

	gtk_text_buffer_get_end_iter(chart_panel->msg_recv_buffer, &end);
	gtk_text_buffer_insert(chart_panel->msg_recv_buffer, &end, msg->chart_info.msg, msg->chart_info.msg_len);

	gtk_text_buffer_get_end_iter(chart_panel->msg_recv_buffer, &end);
	gtk_text_buffer_insert(chart_panel->msg_recv_buffer, &end, "\n\n", -1);
}
