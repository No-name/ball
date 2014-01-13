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

static char * g_login_name;

static BallMainPanel * g_main_panel;

char * ball_get_myself_name()
{
	return g_login_name;
}

void ball_set_myself_name(char * name)
{
	g_login_name = strdup(name);
}

BallMainPanel * ball_get_main_panel()
{
	return g_main_panel;
}

void ball_set_main_panel(BallMainPanel * panel)
{
	g_main_panel = panel;
}

int ball_chart_panel_update_message(struct message_packet * msg)
{
	struct peer_info * peer;

	BallChartPanel * chart_panel;

	list_for_each_entry(peer, &ball_peer_info_set, next)
	{
		if (!strncmp(peer->name, msg->chart_info.from, msg->chart_info.from_len))
		{
			if (peer->chart_panel)
			{
				ball_chart_panel_present_message(peer->chart_panel, msg);
				free(msg);
			}
			else
			{
				list_add_tail(&msg->next, &peer->msg_unshow);
			}

			return TRUE;
		}
	}

	return FALSE;
}

void ball_test_simulate_peer_list()
{
	static char * members[] = {
		"Hokin",
		"Obama",
		"Meilin",
		"Pager",
		NULL
	};

	struct message_packet * msg;
	char * start, * end, * last;
	char * p, * more;
	char * name;
	int name_len;
	int i;

	msg = malloc(sizeof(struct message_packet));
	msg->type = MSG_TYPE_PEER_LIST;
	msg->version = 0x01;

	p = msg->content;
	end = msg->content + MAX_MESSAGE_PACKET_LEN;

	/* step forword to date segment */
	p += MSG_HEAD_LENGTH;

	/* here we reserved position for the more flag */
	more = p;
	p += 1;

	msg->peer_list.start = p;

	last = p;

	for (i = 0; members[i]; i++)
	{
		name = members[i];
		name_len = strlen(name);

		if (p >= end)
			break;

		*p = name_len;
		p += 1;

		if (p + name_len >= end)
			break;

		memcpy(p, name, name_len);
		p += name_len;

		last = p;
	}

	if (members[i])
		msg->peer_list.more = TRUE;
	else
		msg->peer_list.more = FALSE;

	msg->peer_list.end = last;

	package_message(msg);

	message_parse_head(msg);

	list_add_tail(&msg->next, &message_comming);
}

LIST_HEAD(message_peer_list);

int ball_main_panel_update_peer_list(struct message_packet * msg)
{
	struct peer_info * peer;
	char * start, * end;
	char * name;
	int name_len;

	list_add_tail(&msg->next, &message_peer_list);

	if (msg->peer_list.more)
		return TRUE;

	while (!list_empty(&message_peer_list))
	{
		msg = list_first_entry(&message_peer_list, struct message_packet, next);
		list_del(&msg->next);

		start = msg->peer_list.start;
		end = msg->peer_list.end;

		while (start < end)
		{
			name_len = *start;
			start += 1;

			name = start;
			start += name_len;

			peer = malloc(sizeof(struct peer_info));
			memset(peer, 0, sizeof(struct peer_info));

			INIT_LIST_HEAD(&peer->msg_unshow);
			strncpy(peer->name, name, name_len);
			peer->name[name_len] = '\0';

			ball_add_peer_info(ball_get_main_panel(), peer);
		}
	}

	return TRUE;
}

gboolean ball_process_comming_message(gpointer user_data)
{
	int ret;
	struct message_packet * msg;

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

		switch (msg->type)
		{
			case MSG_TYPE_CHART:
				ret = ball_chart_panel_update_message(msg);
				break;
			case MSG_TYPE_PEER_LIST:
				ret = ball_main_panel_update_peer_list(msg);
				break;
			default:
				ret = FALSE;
				break;
		}

		if (ret == FALSE)
			free(msg);
	}

	return TRUE;
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

	//g_timeout_add_seconds(1, ball_chart_moniter_message_comming, NULL);

	gtk_main();

	return 0;
}
#endif

int main(int ac, char ** av)
{
	GtkWidget * window;

	gtk_init(&ac, &av);

	window = ball_login_panel_new();

	gtk_widget_show_all(window);

	g_thread_new("message_proc", ball_process_message_transfor, NULL);
	g_timeout_add_seconds(1, ball_process_comming_message, window);

	gtk_main();

	return 0;
}
