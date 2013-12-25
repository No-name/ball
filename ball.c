#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <assert.h>

#include "list.h"
#include "ball.h"

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
		case MSG_TYPE_LOGIN:
			p = msg->content + MSG_HEAD_LENGTH;

			p += 1;
			n = p + msg->login_info.name_len;
			t = *n;
			*n = 0;
			printf("Login Name: %s\n", p);
			*n = t;

			p = n + 1;
			n = p + msg->login_info.passwd_len;
			t = *n;
			*n = 0;
			printf("Passwd: %s\n", p);
			*n = t;
			break;
		default:
			assert(0 && "message type invalid");
			return;
	}
}

void package_message(struct message_packet * msg)
{
	int len;
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

	p += MSG_HEAD_LENGTH;

	switch (msg->type)
	{
		case MSG_TYPE_LOGIN:
			msg->login_info.name_len = *p;
			p += 1;

			msg->login_info.name = p;
			p += msg->login_info.name_len;

			msg->login_info.passwd_len = *p;
			p += 1;

			msg->login_info.passwd = p;
			break;
		case MSG_TYPE_CHART:
			msg->chart_info.from_len = *p;
			p += 1;

			msg->chart_info.from = p;
			p += msg->chart_info.from_len;

			msg->chart_info.to_len = *p;
			p += 1;

			msg->chart_info.to = p;
			p += msg->chart_info.to_len;

			msg->chart_info.msg_len = ntohs(*(short *)p);
			p += 2;

			msg->chart_info.msg = p;
			break;
		default:
			break;
	}
}

int put_message(struct msg_status * status, int fd)
{
	int wbyte;
	int left = status->left;
	char * p = status->position;

	while (1)
	{
		wbyte = write(fd, p, left);
		if (wbyte == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				status->left = left;
				status->position = p;

				return MSG_SEND_AGAIN;
			}
			
			return MSG_SEND_FAILED;
		}

		p += wbyte;
		left -= wbyte;

		if (left == 0)
		{
			status->left = 0;
			status->position = NULL;

			return MSG_SEND_FINISH;
		}
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
				status->left = msg->length - MSG_HEAD_LENGTH;

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

struct message_packet * initial_new_input_msg(struct msg_status * status)
{
	struct message_packet * packet;
	packet = malloc(sizeof(struct message_packet));
	status->position = packet->content;
	status->left = MSG_HEAD_LENGTH;
	status->phase = MSG_PHASE_GET_HEAD;

	return packet;
}
