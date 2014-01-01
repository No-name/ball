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

#include "gui_client.h"

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

LIST_HEAD(ball_peer_info_set);

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

gboolean ball_chart_panel_update_comming_message(gpointer user_data)
{
	struct message_packet * msg;

	BallChartPanel * chart_panel = BALL_CHART_PANEL(user_data);

	LIST_HEAD(message_wait_proc);

	if (list_empty(&message_comming))
		return TRUE;

	g_mutex_lock(&mutex_for_message_comming);

	list_splice_tail_init(&message_comming, &message_wait_proc);

	g_mutex_unlock(&mutex_for_message_comming);

	while (!list_empty(&message_wait_proc))
	{
		msg = list_first_entry(&message_wait_proc, struct message_packet, next);
		list_del(&msg->next);

		ball_chart_panel_present_message(chart_panel, msg);

		free(msg);
	}

	return TRUE;
}

void ball_chart_panel_process_login(BallChartPanel * chart_panel)
{
	struct message_packet * msg = malloc(sizeof(struct message_packet));

	msg->type = MSG_TYPE_LOGIN;
	msg->version = 0x01;

	msg->login_info.name_len = chart_panel->my_name_len;
	msg->login_info.name = chart_panel->my_name;

	msg->login_info.passwd_len = strlen(PASSWD_DEFAULT);
	msg->login_info.passwd = PASSWD_DEFAULT;

	package_message(msg);

	g_mutex_lock(&mutex_for_message_need_sended);
	list_add_tail(&msg->next, &message_need_sended);
	g_mutex_unlock(&mutex_for_message_need_sended);
}

#if 0
int main(int ac, char ** av)
{
	BallChartPanel * chart_panel;
	GtkWidget * window;

	gtk_init(&ac, &av);

	if (ac != 2)
		return 0;

	window = ball_chart_panel_new();

	//here just for monitor login
	chart_panel = BALL_CHART_PANEL(window);
	chart_panel->my_name = strdup(av[1]);
	chart_panel->my_name_len = strlen(chart_panel->my_name);
	gtk_window_set_title(GTK_WINDOW(window), chart_panel->my_name);
	ball_chart_panel_process_login(chart_panel);

	gtk_widget_show_all(window);

	g_thread_new("message_proc", ball_process_message_transfor, NULL);
	g_timeout_add_seconds(1, ball_chart_panel_update_comming_message, window);
	//g_timeout_add_seconds(1, ball_chart_moniter_message_comming, NULL);

	gtk_main();

	return 0;
}
#endif

int main(int ac, char ** av)
{
	GtkWidget * window;

	gtk_init(&ac, &av);

	window = ball_main_panel_new();

	gtk_widget_show_all(window);

	ball_test_initial_peer_members(BALL_MAIN_PANEL(window));

	gtk_main();

	return 0;
}
