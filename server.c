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

int check_timeout;

struct peer_info {
	struct list_head next;
	time_t time_last;
	int skfd;
	char addr_str[MAX_CLIENT_ADDR_BUF];
	struct sockaddr_in addr;
};

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
					while (1)
					{
						rbyte = read(client_fd, msg_buffer, MAX_MSG_BUFFER_LEN);
						if (rbyte == -1)
						{
							if (errno == EWOULDBLOCK || errno == EAGAIN)
							{
								list_del(&peer->next);
								peer->time_last = time(NULL);

								list_add_tail(&peer->next, &timeout_list);
								break;
							}

							epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
							list_del(&peer->next);
							close(peer->skfd);
							peer_info_pool[client_fd] = NULL;

							free(peer);
							break;
						}

						if (rbyte == 0)
							break;

						msg_buffer[rbyte] = '\0';
						fprintf(stdout, "[%d msg from %s] %s\n", peer->skfd, peer->addr_str, msg_buffer);

						wbyte = write(client_fd, msg_buffer, rbyte);
					}

				}

				if (ep_responds[i].events & EPOLLOUT)
				{
					printf("Client with POLLOUT event\n");

				}

				if (ep_responds[i].events & EPOLLHUP)
				{
					printf("Client with POLLHUP event\n");
					epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);

					fprintf(stdout, "socket with fd %d leave out\n", client_fd);
				}
			}
		}

		sigprocmask(SIG_SETMASK, &sig_orig, NULL);
	}
}
