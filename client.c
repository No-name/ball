#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 25678

#define PASSWD_DEFAULT "123456"

#define MAX_MSG_BUFFER_LEN 100

struct message_send_queue {
	struct list_head msg_list;
	struct msg_status status;
	int (*put_msg)(struct msg_status * status, int skfd);
};

struct message_endpoint {
	int skfd;

	struct message_send_queue output_queue;

	struct list_head msg_input_list;
	struct msg_status input_status;
	struct message_packet * msg_input;
	int (*get_msg)(struct message_packet * msg, struct msg_status * status, int skfd);
};

int send_message(struct message_send_queue * queue, int skfd)
{
	int ret;
	struct message_packet * msg;

	msg = list_first_entry(&queue->msg_list, struct message_packet, next);

	while (1)
	{
		if (queue->status.left)
		{
			ret = queue->put_msg(&queue->status, skfd);
			if (ret == MSG_SEND_FINISH)
			{
				list_del(&msg->next);
				free(msg);
			}
			else
			{
				return ret;
			}
		}

		msg = list_first_entry(&queue->msg_list, struct message_packet, next);
		if (msg == NULL)
			break;

		queue->status.position = msg->content;
		queue->status.left = msg->length;
	}

	return MSG_SEND_FINISH;
}

void present_message(struct message_packet * msg)
{
	char * p, * n, t;

	switch (msg->type)
	{
		case MSG_TYPE_CHART:
			p = msg->content + MSG_HEAD_LENGTH;

			p += 1;
			n = p + msg->chart_info.from_len;
			t = *n;
			*n = 0;
			printf("From: %s\n", p);
			*n = t;

			p = n + 1;
			n = p + msg->chart_info.to_len;
			t = *n;
			*n = 0;
			printf("To: %s\n", p);
			*n = t;

			p = n + 2;
			n = p + msg->chart_info.msg_len;
			t = *n;
			*n = 0;
			printf("Content:\n\t%s\n", p);
			*n = t;

			break;
		default:
			assert(0 && "message type invalid");
			return;
	}
}

void package_message(struct message_packet * msg)
{
	char * p = msg->content;

	*(int *)p = htonl(msg->version);
	p += 4;

	*(int *)p = htonl(msg->type);
	p += 4;

	//reserve the head length
	*(int *)p = 0;
	p += 4;

	switch (msg->type)
	{
		case MSG_TYPE_LOGIN:
			len = msg->login_info.name_len;
			*p = len;
			p += 1;

			memcpy(p, msg->login_info.name, len);
			p += len;

			len = msg->login_info.passwd_len;
			*p = len;
			p += 1;

			memcpy(p, msg->login_info.passwd, len);
			p += len;
			break;

		case MSG_TYPE_CHART:
			len = msg->chart_info.from_len;
			*p = len;
			p += 1;

			memcpy(p, msg->chart_info.from, len);
			p += len;

			len = msg->chart_info.to_len;
			*p = len;
			p += 1;

			memcpy(p, msg->chart_info.to, len);
			p += len;

			len = msg->chart_info.msg_len;

			*(unsigned short *)p = htons(len);
			p += 2;

			memcpy(p, msg->chart_info.msg, len);
			p += len;
			break;

		default:
			assert(0 && "message type invalid");
			msg->length = 0;
			return;
	}

	len = p - msg->content;

	((int *)msg->content)[2] = htonl(len);

	msg->length = len;

	return;
}

int main(int ac, char ** av)
{
	struct epoll_event ep_responds[MAX_CLIENT_EPOLL_EVENT];
	struct epoll_event ep_inject;
	struct sockaddr_in server_addr;
	struct message_endpoint client_endpoint;
	struct message_packet * msg;
	int client_fd;

	if (ac != 2)
	{
		printf("Usage: %s username\n", av[0]);
		exit(0);
	}

	epfd = epoll_create(1);

	memset(&client_endpoint, 0, sizeof(struct message_endpoint));
	INIT_LIST_HEAD(&client_endpoint.output_queue.msg_list);
	client_endpoint.output_queue.put_msg = put_message;

	INIT_LIST_HEAD(&client_endpoint.msg_input_list);
	client_endpoint.msg_input = initial_new_input_msg(&client_endpoint.msg_input);
	client_endpoint.get_msg = get_message;


	client_fd = socket(AF_INET, SOCK_STREAM, 0);

	flag = fcntl(client_fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(client_fd, F_SETFL, flag);

	ep_inject.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ep_inject.data.fd = client_fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ep_inject);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

	ret = connect(client_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
	if (ret == -1)
	{
		fprintf(stderr, "connect failed %s\n", strerror(errno));
		exit(0);
	}

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

	while (1)
	{
		ret = epoll_wait(epfd, ep_responds, MAX_CLIENT_EPOLL_EVENT, -1);
		if (ret == -1)
		{
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
					ret = client_endpoint.get_msg(client_endpoint.msg_input, &client_endpoint->input_status, client_endpoint->skfd);
					if (ret == MSG_GET_FAILED)
					{
						//here the skfd read failed, maybe we need a reconnection to the 
						//server, we must deal with it
					}
					else if (ret == MSG_GET_FINISH)
					{
						list_add_tail(&client_endpoint.msg_input.next, &client_endpoint.msg_input_list);
						client_endpoint->msg_input = initial_new_input_msg(&client_endpoint->input_status);
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

		while ((msg = list_first_entry(&client_endpoint->msg_input_list, struct message_packet, next)))
		{
			list_del(&msg->next);
			present_message(msg);

			free(msg);
		}
	}


	for (i = 1; i < 100; ++i)
	{
		sleep(1);
		sprintf(msg_buffer, "[%d] send %d msgs to server", pid, i);

		write(client_fd, msg_buffer, strlen(msg_buffer));

		rbyte = read(client_fd, respond_buffer, MAX_MSG_BUFFER_LEN);
		respond_buffer[rbyte] = '\0';

		fprintf(stdout, "%s\n", respond_buffer);
	}
}
