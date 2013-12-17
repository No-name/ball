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

#define MAX_MSG_BUFFER_LEN 100

int main()
{
	struct sockaddr_in server_addr;
	char msg_buffer[MAX_MSG_BUFFER_LEN];
	char respond_buffer[MAX_MSG_BUFFER_LEN];
	int pid;

	int client_fd;
	int i, ret, rbyte;

	pid = getpid();

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
