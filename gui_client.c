#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#include "list.h"
#include "ball.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 25678

#define MAX_CLIENT_EPOLL_EVENT 10

#define PASSWD_DEFAULT "123456"

struct message_endpoint {
	int skfd;

	struct message_send_queue output_queue;

	struct list_head msg_input_list;
	struct msg_status input_status;
	struct message_packet * msg_input;
	int (*get_msg)(struct message_packet * msg, struct msg_status * status, int skfd);
};

LIST_HEAD(message_need_sended);
GMutex mutex_for_message_need_sended;

LIST_HEAD(message_comming);
GMutex mutex_for_message_comming;

gpointer ball_process_message_transfor(gpointer user_data)
{
	struct epoll_event ep_responds[MAX_CLIENT_EPOLL_EVENT];
	struct epoll_event ep_inject;
	struct sockaddr_in server_addr;
	struct message_endpoint client_endpoint;
	struct message_packet * msg;
	int client_fd, epfd;
	int ret;
	int flag;
	int i;

	epfd = epoll_create(1);

	memset(&client_endpoint, 0, sizeof(struct message_endpoint));
	INIT_LIST_HEAD(&client_endpoint.output_queue.msg_list);
	client_endpoint.output_queue.put_msg = put_message;

	INIT_LIST_HEAD(&client_endpoint.msg_input_list);
	client_endpoint.msg_input = initial_new_input_msg(&client_endpoint.input_status);
	client_endpoint.get_msg = get_message;

	client_fd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

	ret = connect(client_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
	if (ret == -1)
	{
		fprintf(stderr, "connect failed %s\n", strerror(errno));
		exit(0);
	}

	flag = fcntl(client_fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(client_fd, F_SETFL, flag);

	ep_inject.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ep_inject.data.fd = client_fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ep_inject);

	client_endpoint.skfd = client_fd;

	while (1)
	{
		//first we should check wether there are new messages
		//need to be sended, and send it
		if (!list_empty(&message_need_sended))
		{
			g_mutex_lock(&mutex_for_message_need_sended);
			list_splice_tail_init(&message_need_sended, &client_endpoint.output_queue.msg_list);
			g_mutex_unlock(&mutex_for_message_need_sended);

			send_message(&client_endpoint.output_queue, client_endpoint.skfd);
		}

		//we will wait up to 1s if no other event wake up
		ret = epoll_wait(epfd, ep_responds, MAX_CLIENT_EPOLL_EVENT, 1000);
		if (ret == -1)
		{
			printf("epoll wait come to error");
			if (errno == EINTR)
				continue;
			else
				raise(SIGTERM);
		}

		if (ret == 0)
			continue;

		for (i = 0; i < ret; ++i)
		{
			if (ep_responds[i].data.fd == client_fd)
			{
				if (ep_responds[i].events & EPOLLIN)
				{
					ret = client_endpoint.get_msg(client_endpoint.msg_input, &client_endpoint.input_status, client_endpoint.skfd);
					if (ret == MSG_GET_FAILED)
					{
						//here the skfd read failed, maybe we need a reconnection to the 
						//server, we must deal with it
					}
					else if (ret == MSG_GET_FINISH)
					{
						list_add_tail(&client_endpoint.msg_input->next, &client_endpoint.msg_input_list);
						client_endpoint.msg_input = initial_new_input_msg(&client_endpoint.input_status);
					}
				}

				if (ep_responds[i].events & EPOLLOUT)
				{
					ret = send_message(&client_endpoint.output_queue, client_endpoint.skfd);
					if (ret == MSG_SEND_FAILED)
					{
						//here the skfd just write failed, maybe the connection is
						//broken, we need deal with it
					}
				}
			}
		}

		//if we recive some message, just move they to comming list
		if (!list_empty(&client_endpoint.msg_input_list))
		{
			g_mutex_lock(&mutex_for_message_comming);
			list_splice_tail_init(&client_endpoint.msg_input_list, &message_comming);
			g_mutex_unlock(&mutex_for_message_comming);
		}
	}

	return 0;
}

//funciton used to check wether new messages
//have come, and deal with it
gboolean ball_check_recive_message(gpointer user_data)
{
	g_message("No message comming");
	return TRUE;
}

#define BALL_TYPE_CHART_PANEL (ball_chart_panel_get_type())
#define BALL_CHART_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BALL_TYPE_CHART_PANEL, BallChartPanel))
#define BALL_IS_CHART_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BALL_TYPE_CHART_PANEL))
#define BALL_CHART_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), BALL_TYPE_CHART_PANEL, BallChartPanelClass))
#define BALL_IS_CHART_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), BALL_TYPE_CHART_PANEL))
#define BALL_CHART_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BALL_TYPE_CHART_PANEL, BallChartPanelClass))

typedef struct _BallChartPanel BallChartPanel;
typedef struct _BallChartPanelClass BallChartPanelClass;

struct _BallChartPanel
{
	GtkWindow parent;

	GtkWidget * msg_recv, * msg_send;
	GtkWidget * paned;
	GtkTextBuffer * msg_recv_buffer, * msg_send_buffer;
	GtkTextTagTable * msg_tag_table;
	GtkWidget * msg_send_button_box, * send_button, * clear_button;
	GtkWidget * msg_send_view;
	GtkWidget * msg_reciver_view;
	GtkWidget * reciver_label, * recivers_combo_box;

	char * my_name;
	int my_name_len;
};

struct _BallChartPanelClass
{
	GtkWindowClass parent_class;
};

void ball_chart_panel_send_message(GtkWidget * button, BallChartPanel * chart_panel)
{
	struct message_packet * msg;

	int msg_len;
	GtkTextIter start, end;
	gchar * message_content, * send_to;

	send_to = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(chart_panel->recivers_combo_box));

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

	msg->chart_info.from_len = chart_panel->my_name_len;
	msg->chart_info.from = chart_panel->my_name;

	msg->chart_info.to_len = strlen(send_to);
	msg->chart_info.to = send_to;

	msg->chart_info.msg_len = msg_len;
	msg->chart_info.msg = message_content;

	package_message(msg);

	g_mutex_lock(&mutex_for_message_need_sended);
	list_add_tail(&msg->next, &message_need_sended);
	g_mutex_unlock(&mutex_for_message_need_sended);

out:

	g_free(message_content);
	g_free(send_to);
}

void ball_chart_panel_clear_message(GtkWidget * button, GtkWidget * chart_panel)
{

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

	chart_panel->msg_recv = gtk_text_view_new_with_buffer(chart_panel->msg_recv_buffer);
	gtk_paned_add1(GTK_PANED(chart_panel->paned), chart_panel->msg_recv);

	chart_panel->msg_send_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_paned_add2(GTK_PANED(chart_panel->paned), chart_panel->msg_send_view);

	chart_panel->msg_reciver_view = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_view), chart_panel->msg_reciver_view, FALSE, FALSE, 0);

	chart_panel->reciver_label = gtk_label_new("Msg send to :");
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_reciver_view), chart_panel->reciver_label, FALSE, FALSE, 0);

	chart_panel->recivers_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_reciver_view), chart_panel->recivers_combo_box, TRUE, TRUE, 0);

	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(chart_panel->recivers_combo_box), NULL, "Jerry");

	chart_panel->msg_send = gtk_text_view_new_with_buffer(chart_panel->msg_send_buffer);
	gtk_box_pack_start(GTK_BOX(chart_panel->msg_send_view), chart_panel->msg_send, TRUE, TRUE, 0);

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

int main(int ac, char ** av)
{
	GtkWidget * window;

	gtk_init(&ac, &av);

	window = ball_chart_panel_new();

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
