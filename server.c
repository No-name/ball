#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 25678

#define MAX_EPOLL_EVENT 100
#define MAX_MSG_BUFFER_LEN 100
#define MAX_CLIENT_ADDR_BUF 100

#define MAX_PEER_INFO_COUNT 300

#define TIME_IDEL_MAX 20

#define MAX_MESSAGE_PACKET_LEN 1024

#define MSG_HEAD_LENGTH 12


int check_timeout;

enum {
	MSG_GET_FAILED,
	MSG_GET_AGAIN,
	MSG_GET_FINISH,
};

enum {
	MSG_PHASE_GET_HEAD,
	MSG_PHASE_GET_BODY,
};

enum {
	MSG_TYPE_LOGIN,
	MSG_TYPE_CHART,
};

struct message_packet {
	struct list_head next;
	int msg_is_ok;

	int type;
	int length;
	int version;
	
	union {
		struct {
			int name_len;
			int passwd_len;
			char * name;
			char * passwd;
		} login_info;

		struct {
			int from_len;
			int to_len;
			char * msg_from;
			char * msg_to;
		} chart_info;
	};

	char content[MAX_MESSAGE_PACKET_LEN];
};

struct msg_status {
	char * position;
	int phase;
	int left;
};

struct peer_info {
	struct list_head next;
	time_t time_last;
	int skfd;
	char addr_str[MAX_CLIENT_ADDR_BUF];
	struct sockaddr_in addr;

	struct message_packet * msg_input;
	struct list_head msg_output_list;
	struct msg_status input_status;
	struct msg_status output_status;
	int (*get_msg)(struct message_packet * msg, struct msg_status * status, int skfd);
	int (*put_msg)(struct msg_status * status, int skfd);
};

extern int get_message(struct message_packet * msg, struct msg_status * status, int fd);

struct peer_info * peer_info_pool[MAX_PEER_INFO_COUNT];

struct list_head timeout_list;

struct peer_info * peer_info_head;

int epfd;

void init_routine()
{
	check_timeout = 0;

	INIT_LIST_HEAD(&timeout_list);
}

void clear_outtime_peer()
{
	struct peer_info * peer;
	time_t now = time(NULL);

	while (!list_empty(&timeout_list)) 
	{
		peer = list_first_entry(&timeout_list, struct peer_info, next);

		if (now - peer->time_last > TIME_IDEL_MAX)
		{
			printf("I clean the fd from %s\n", peer->addr_str);

			list_del(&peer->next);

			peer_info_pool[peer->skfd] = NULL;

			epoll_ctl(epfd, EPOLL_CTL_DEL, peer->skfd, NULL);

			close(peer->skfd);

			free(peer);

			continue;
		}

		break;
	}
}

void sig_hand_alarm(int sig)
{
	printf("I am timeout\n");
	check_timeout = 1;
}

struct message_packet * initial_new_input_msg(struct msg_status * status)
{
	struct message_packet * packet;
	packet = malloc(sizeof(struct message_packet));
	status->position = packet->content;
	status->left = MSG_HEAD_LENGTH;
	status->phase = MSG_PHASE_GET_HEAD;

	return packet;
}

void process_message(struct message_packet * msg, struct peer_info * peer)
{

}

void destroy_peer_info(struct peer_info * peer)
{
	free(peer->msg_input);

	//clear the output msg packets
	
	free(peer);
}

int main()
{
	sigset_t sig_mask, sig_orig;
	struct itimerval timerval;
	struct peer_info * peer;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
	char msg_buffer[MAX_MSG_BUFFER_LEN];
	char client_addr_buf[MAX_CLIENT_ADDR_BUF];
	int client_fd;
	int listen_sock;
	int len;
	int flag;
	int i, ret, rbyte, wbyte;
	time_t now;

	struct epoll_event ep_responds[MAX_EPOLL_EVENT];
	struct epoll_event ep_inject;

	signal(SIGALRM, sig_hand_alarm);

	init_routine();

	sigaddset(&sig_mask, SIGALRM);

	epfd = epoll_create(100);

	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	flag = fcntl(listen_sock, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(listen_sock, F_SETFL, flag);

	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr); 
	server_addr.sin_port = htons(SERVER_PORT);

	if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
	{
		fprintf(stderr, "Bind failed %s\n", strerror(errno));
		exit(0);
	}

	ep_inject.events = EPOLLIN | EPOLLET;
	ep_inject.data.fd = listen_sock;

	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &ep_inject);

	listen(listen_sock, 10);

	timerval.it_interval.tv_sec = 1;
	timerval.it_interval.tv_usec = 0;

	timerval.it_value = timerval.it_interval;

	setitimer(ITIMER_REAL, &timerval, NULL);

	while (1)
	{
		if (check_timeout)
		{
			check_timeout = 0;


			sigprocmask(SIG_BLOCK, &sig_mask, &sig_orig);
			clear_outtime_peer();
			sigprocmask(SIG_SETMASK, &sig_orig, NULL);
		}

		ret = epoll_wait(epfd, ep_responds, MAX_EPOLL_EVENT, -1);
		if (ret == -1)
		{
			printf("epoll_wait error (%d) %s\n", errno, strerror(errno));
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
			if (ep_responds[i].data.fd == listen_sock)
			{
				//here some body want to connect to us

				while (1)
				{
					now = time(NULL);
					peer = malloc(sizeof(struct peer_info));
					memset(peer, 0, sizeof(struct peer_info));

					peer->msg_input = initial_new_input_msg(&peer->input_status);
					INIT_LIST_HEAD(&peer->msg_output_list);
					peer->get_msg = get_message;

					len = sizeof(struct sockaddr);
					client_fd = accept(listen_sock, (struct sockaddr *)&client_addr, &len);
					if (client_fd == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;

						raise(SIGTERM);
					}

					flag = fcntl(client_fd, F_GETFL);
					flag |= O_NONBLOCK;
					fcntl(client_fd, F_SETFL, flag);

					ep_inject.events = EPOLLIN | EPOLLET | EPOLLHUP;
					ep_inject.data.fd = client_fd;

					epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ep_inject);

					peer->skfd = client_fd;
					peer->addr = client_addr;
					inet_ntop(AF_INET, &client_addr.sin_addr, peer->addr_str, MAX_CLIENT_ADDR_BUF);
					sprintf(peer->addr_str + strlen(peer->addr_str), ":%d", ntohs(client_addr.sin_port));
					peer->time_last = now;

					peer_info_pool[client_fd] = peer;

					list_add_tail(&peer->next, &timeout_list);

					fprintf(stdout, "Connection from %s\n", peer->addr_str);
				}
			}
			else
			{
				//here we need respond the fd some message
				//we just an echo server

				client_fd = ep_responds[i].data.fd;

				peer = peer_info_pool[client_fd];

				if (ep_responds[i].events & EPOLLIN)
				{
					printf("Client with POLLIN event\n");

					ret = peer->get_msg(peer->msg_input, &peer->input_status, peer->skfd);
					if (ret == MSG_GET_FAILED)
					{
						epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
						list_del(&peer->next);
						close(peer->skfd);
						peer_info_pool[client_fd] = NULL;

						destroy_peer_info(peer);
					}
					else
					{
						list_del(&peer->next);
						peer->time_last = time(NULL);
						list_add_tail(&peer->next, &timeout_list);

						if (ret == MSG_GET_FINISH)
						{
							process_message(peer->msg_input, peer);

							peer->msg_input = initial_new_input_msg(&peer->input_status);
						}
					}
				}

				//here we output the message to the peer client
				if (ep_responds[i].events & EPOLLOUT)
				{
					printf("Client with POLLOUT event\n");
				}
			}
		}

		sigprocmask(SIG_SETMASK, &sig_orig, NULL);
	}
}

void message_parse_head(struct message_packet * msg)
{
	char * p = msg->content;

	msg->version = ntohl(*(int *)p);
	p += 4;

	msg->type = ntohl(*(int *)p);
	p += 4;

	msg->length = ntohl(*(int *)p);
}

void message_parse_body(struct message_packet * msg)
{
	char * p = msg->content;

	switch (msg->type)
	{
		case MSG_TYPE_LOGIN:
			break;
		case MSG_TYPE_CHART:
			break;
		default:
			break;
	}
}

int get_message(struct message_packet * msg, struct msg_status * status, int fd)
{
	int rbyte;
	char * p;
	int left;

	p = status->position;
	left = status->left;

	while (1)
	{
		rbyte = read(fd, p, left);
		if (rbyte == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}

			return MSG_GET_FAILED;
		}

		p += rbyte;
		left -= rbyte;

		if (left == 0)
		{
			if (status->phase == MSG_PHASE_GET_HEAD)
			{
				message_parse_head(msg);
				
				status->position = p;
				status->left = msg->length;

				status->phase = MSG_PHASE_GET_BODY;

				//we assgin the position and left again
				//the p already eq the position
				left = status->left;
			}
			else
			{
				message_parse_body(msg);

				return MSG_GET_FINISH;
			}
		}
	}

	status->position = p;
	status->left = left;

	return MSG_GET_AGAIN;
}
