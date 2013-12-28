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

#include <assert.h>

#include "list.h"
#include "ball.h"

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 25678

#define MAX_EPOLL_EVENT 100
#define MAX_CLIENT_ADDR_BUF 100

#define MAX_PEER_INFO_COUNT 300

#define TIME_IDEL_MAX 20


int check_timeout;

struct account_info {
	struct hlist_node hnext;
	struct peer_info * conn;

	int name_len;
	char name[MAX_ACCOUNT_NAME_LEN];
};

struct peer_info {
	struct list_head next;
	time_t time_last;
	int skfd;
	char addr_str[MAX_CLIENT_ADDR_BUF];
	struct sockaddr_in addr;

	struct message_send_queue output_queue;

	struct message_packet * msg_input;
	struct msg_status input_status;

	//obsolete
	struct list_head msg_output_list;
	struct msg_status output_status;
	int (*put_msg)(struct msg_status * status, int skfd);

	int (*get_msg)(struct message_packet * msg, struct msg_status * status, int skfd);
};

int get_message(struct message_packet * msg, struct msg_status * status, int fd);
int put_message(struct msg_status * status, int fd);
void init_account_info_hash_table();

struct peer_info * peer_info_pool[MAX_PEER_INFO_COUNT];

struct list_head timeout_list;

struct peer_info * peer_info_head;

int epfd;

void init_routine()
{
	check_timeout = 0;

	INIT_LIST_HEAD(&timeout_list);

	init_account_info_hash_table();
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

int account_name_hash(char * name, int len)
{
	return 0;
}

struct hlist_head * gpstAccountInfoHashTable;
#define ACCOUNT_INFO_HASH_SLOT_MAX 1000

void init_account_info_hash_table()
{
	int i;
	struct hlist_head * htable;

	htable = malloc(sizeof(struct hlist_head) * ACCOUNT_INFO_HASH_SLOT_MAX);

	for (i = 0; i < ACCOUNT_INFO_HASH_SLOT_MAX; ++i)
	{
		INIT_HLIST_HEAD(htable + i);
	}

	gpstAccountInfoHashTable = htable;

	assert(gpstAccountInfoHashTable && "Account hash table alloc failed\n");
}

void add_account_info_to_db(struct account_info * account)
{
	struct hlist_head * hslot;

	int hash = account_name_hash(account->name, account->name_len);

	hslot = gpstAccountInfoHashTable + hash;

	hlist_add_head(&account->hnext, hslot);
}

struct account_info * get_account_from_db(char * name, int len)
{
	struct hlist_head * hslot;
	struct hlist_node * hnode;
	struct account_info * account;

	int hash = account_name_hash(name, len);

	hslot = gpstAccountInfoHashTable + hash;

	hlist_for_each_entry(account, hnode, hslot, hnext)
	{
		if (!strncmp(account->name, name, account->name_len < len ? account->name_len : len))
		{
			return account;
		}
	}

	return NULL;
}

void process_message(struct message_packet * msg, struct peer_info * peer)
{
	struct account_info * account;

	present_message(msg);

	switch (msg->type)
	{
		case MSG_TYPE_LOGIN:
			account = malloc(sizeof(struct account_info));
			memcpy(account->name, msg->login_info.name, msg->login_info.name_len);
			account->name_len = msg->login_info.name_len;
			account->conn = peer;

			add_account_info_to_db(account);

			free(msg);
			break;
		case MSG_TYPE_CHART:
			account = get_account_from_db(msg->chart_info.to, msg->chart_info.to_len);
			if (!account)
			{
				free(msg);
			}
			else
			{
				list_add_tail(&msg->next, &account->conn->output_queue.msg_list);
				send_message(&account->conn->output_queue, account->conn->skfd);
			}

			break;
		default:
			assert(0 && "message type invalid");
			free(msg);
			return;
	}
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
	int client_fd;
	int listen_sock;
	int len;
	int flag;
	int i, ret, total;
	time_t now;

	struct epoll_event ep_responds[MAX_EPOLL_EVENT];
	struct epoll_event ep_inject;

	signal(SIGALRM, sig_hand_alarm);

	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGALRM);

	init_routine();

	epfd = epoll_create(100);

	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	flag = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

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

		for (i = 0, total = ret; i < total; ++i)
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
					peer->get_msg = get_message;

					INIT_LIST_HEAD(&peer->output_queue.msg_list);
					peer->output_queue.put_msg = put_message;

					len = sizeof(struct sockaddr);
					client_fd = accept(listen_sock, (struct sockaddr *)&client_addr, (socklen_t *)&len);
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
						printf("Get message failed\n");
						epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
						list_del(&peer->next);
						close(peer->skfd);
						peer_info_pool[client_fd] = NULL;

						destroy_peer_info(peer);

						printf("Successfully process the failed state\n");
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

					send_message(&peer->output_queue, peer->skfd);
				}
			}
		}

		sigprocmask(SIG_SETMASK, &sig_orig, NULL);
	}
}

#if 0
void send_message(struct peer_info * peer)
{
	int ret = MSG_SEND_FINISH;
	struct message_packet * msg;

	msg = list_first_entry(&peer->msg_output_list, struct message_packet, next);

	while (1)
	{
		if (peer->output_status.left)
		{
			ret = peer->put_msg(&peer->output_status, peer->skfd);
			if (ret == MSG_SEND_FINISH)
			{
				list_del(&msg->next);
				free(msg);
			}
			else if (ret == MSG_SEND_AGAIN)
			{
				return;
			}
			else
			{
				//there the peer is gone, just remove the peer
				return;
			}
		}

		if (list_empty(&peer->msg_output_list))
			break;

		msg = list_first_entry(&peer->msg_output_list, struct message_packet, next);

		peer->output_status.position = msg->content;
		peer->output_status.left = msg->length;
	}
}
#endif
