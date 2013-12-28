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

static int flag_gen_msg;

void sig_hand_alarm(int sig)
{
	flag_gen_msg = 1;

	printf("Timeout need generate msg\n");
}

//use for monilate the chart message generate
struct message_packet * generate_chart_msg(char * msg_from)
{
	static char * msg_reciver[] = {
		"Brush",
		"Stevens",
		"Cooke",
		"Riched",
	};

	static int msg_index;

	int i;
	struct message_packet * msg;
	char msg_to[MAX_ACCOUNT_NAME_LEN];
	char msg_content[MAX_MSG_CONTENT_LEN];

	i = msg_index % 4;
	if (!strcmp(msg_from, msg_reciver[i]))
		i = (i + 1) % 4;

	sprintf(msg_to, "%s", msg_reciver[i]);
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

	msg_index++;

	return msg;
}

int main(int ac, char ** av)
{
	sigset_t sig_mask, sig_orig;
	struct itimerval timerval;
	struct epoll_event ep_responds[MAX_CLIENT_EPOLL_EVENT];
	struct epoll_event ep_inject;
	struct sockaddr_in server_addr;
	struct message_endpoint client_endpoint;
	struct message_packet * msg;
	int client_fd, epfd;
	int ret;
	int flag;
	int i;

	if (ac != 2)
	{
		printf("Usage: %s username\n", av[0]);
		exit(0);
	}

	signal(SIGALRM, sig_hand_alarm);

	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGALRM);

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

	msg = malloc(sizeof(struct message_packet));
	msg->type = MSG_TYPE_LOGIN;
	msg->version = 0x01;

	msg->login_info.name_len = strlen(av[1]);
	msg->login_info.name = av[1];

	msg->login_info.passwd_len = strlen(PASSWD_DEFAULT);
	msg->login_info.passwd = PASSWD_DEFAULT;

	package_message(msg);

	list_add_tail(&msg->next, &client_endpoint.output_queue.msg_list);
	send_message(&client_endpoint.output_queue, client_endpoint.skfd);

	printf("We do here\n");

	timerval.it_interval.tv_sec = 1;
	timerval.it_interval.tv_usec = 0;
	timerval.it_value = timerval.it_interval;

	setitimer(ITIMER_REAL, &timerval, NULL);

	while (1)
	{
		if (flag_gen_msg)
		{
			//this is just from test to generate message
			flag_gen_msg = 0;

			sigprocmask(SIG_BLOCK, &sig_mask, &sig_orig);

			printf("Send message start\n");

			msg = generate_chart_msg(av[1]);
			list_add_tail(&msg->next, &client_endpoint.output_queue.msg_list);
			send_message(&client_endpoint.output_queue, client_endpoint.skfd);

			printf("Send message finish\n");

			sigprocmask(SIG_SETMASK, &sig_orig, NULL);
		}

		printf("Now we wait for some respond\n");

		ret = epoll_wait(epfd, ep_responds, MAX_CLIENT_EPOLL_EVENT, -1);
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

		sigprocmask(SIG_BLOCK, &sig_mask, &sig_orig);

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

		while (!list_empty(&client_endpoint.msg_input_list))
		{
			msg = list_first_entry(&client_endpoint.msg_input_list, struct message_packet, next);
			list_del(&msg->next);
			present_message(msg);

			free(msg);
		}

		sigprocmask(SIG_SETMASK, &sig_orig, NULL);
	}

	return 0;
}
