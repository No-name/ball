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

#define TIME_IDEL_MAX 100000


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
	int (*get_msg)(struct message_packet * msg, struct msg_status * status, int skfd);

	struct account_info * account;
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

void destroy_peer_info(struct peer_info * peer)
{
	struct account_info * account;

	close(peer->skfd);
	list_del(&peer->next);
	peer_info_pool[peer->skfd] = NULL;
	epoll_ctl(epfd, EPOLL_CTL_DEL, peer->skfd, NULL);

	if ((account = peer->account))
	{
		account->conn = NULL;
		account_remove_from_db(account);
	}

	free(peer->msg_input);

	//clear the output msg packets
	//also if the account have login, remove the account from db
	
	free(peer);
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


			destroy_peer_info(peer);

			continue;
		}

		break;
	}
}

void sig_hand_alarm(int sig)
{
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

void account_remove_from_db(struct account_info * account)
{
	struct peer_info * peer;

	/* we should notify other the account has gone */

	hlist_del(&account->hnext);

	if ((peer = account->conn))
	{
		peer->account = NULL;
		destroy_peer_info(peer);
	}

	free(account);
}

void account_add_into_db(struct account_info * account)
{
	int hash;
	struct hlist_head * hslot;

	hash = account_name_hash(account->name, account->name_len);
	hslot = gpstAccountInfoHashTable + hash;

	hlist_add_head(&account->hnext, hslot);

	/* we should notify other the account login */
}

struct account_info * account_get_from_db(char * name, int len)
{
	struct hlist_head * hslot;
	struct hlist_node * hnode;
	struct account_info * account;
	int hash;

	hash = account_name_hash(name, len);
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

int ball_pack_relationship_packet(char * account_name, const char * member_name)
{
	struct account_info * account;
	struct message_packet * msg;
	struct message_send_queue * output_queue;
	int peer_skfd;
	char * p;
	const char * name = member_name;
	int name_len = strlen(member_name);

	account = account_get_from_db(account_name, strlen(account_name));
	if (!account)
		return FALSE;

	output_queue = &account->conn->output_queue;
	peer_skfd = account->conn->skfd;

	msg = malloc(sizeof(struct message_packet));
	msg->type = MSG_TYPE_PEER_LIST;
	msg->version = 0x01;

	p = msg->content;

	p += MSG_HEAD_LENGTH;

	*p = name_len;
	p += 1;

	memcpy(p, name, name_len);
	p += name_len;

	msg->length = p - msg->content;

	message_package_head(msg);

	list_add_tail(&msg->next, &output_queue->msg_list);
	send_message(output_queue, peer_skfd);

	return TRUE;
}

void message_proc_login(struct message_packet * msg, struct peer_info * peer)
{
	struct account_info * account;
	char * p = MESSAGE_BODY(msg);
	char * name, * passwd;
	int name_len, passwd_len;

	/* first parse the message */
	name_len = *p;
	p += 1;

	name = p;
	p += name_len;

	passwd_len = *p;
	p += 1;

	passwd = p;

	/* then we process the message */
	account = malloc(sizeof(struct account_info));
	memcpy(account->name, name, name_len);
	account->name[name_len] = '\0';
	account->name_len = name_len;

	/* each should know the others */
	account->conn = peer;
	peer->account = account;

	/* info the other routine we online */
	account_add_into_db(account);

	/* respond with the member list */
	ball_respond_relationship(account->name);
}

void process_message(struct message_packet * msg, struct peer_info * peer)
{
	struct account_info * account;

	present_message(msg);

	switch (msg->type)
	{
		case MSG_TYPE_LOGIN:
			message_proc_login(msg, peer);
			free(msg);

			break;
		case MSG_TYPE_CHART:
			account = account_get_from_db(msg->chart_info.to, msg->chart_info.to_len);
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

					while (1)
					{
						ret = peer->get_msg(peer->msg_input, &peer->input_status, peer->skfd);
						if (ret == MSG_GET_FAILED)
						{
							printf("Get message failed\n");

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
								continue;
							}
						}

						break;
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
