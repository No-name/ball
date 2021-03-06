#ifndef _BALL_H_
#define _BALL_H_

#define MAX_MESSAGE_PACKET_LEN 1024
#define MAX_ACCOUNT_NAME_LEN 64
#define MAX_MSG_CONTENT_LEN 512

#define MSG_HEAD_LENGTH 12

enum {
	MSG_GET_FAILED,
	MSG_GET_AGAIN,
	MSG_GET_FINISH,

	MSG_SEND_FAILED,
	MSG_SEND_AGAIN,
	MSG_SEND_FINISH,
};

enum {
	MSG_PHASE_GET_HEAD,
	MSG_PHASE_GET_BODY,
};

enum {
	MSG_TYPE_LOGIN,
	MSG_TYPE_CHART,
	MSG_TYPE_PEER_LIST,
	MSG_TYPE_LOGIN_RESPOND,
};

enum {
	BALL_LOGIN_ACCOUNT_FAILED,
	BALL_LOGIN_PASSWD_FAILED,
	BALL_LOGIN_FAILED,
	BALL_LOGIN_SUCCESS,
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
			int msg_len;
			char * from;
			char * to;
			char * msg;
		} chart_info;

		struct {
			int more; //message not complete, need more data
			char * start;
			char * end;
		} peer_list;
	};

	char content[MAX_MESSAGE_PACKET_LEN];
};

#define MESSAGE_BODY(msg) ((msg)->content + MSG_HEAD_LENGTH)

struct msg_status {
	char * position;
	int phase;
	int left;
};

struct message_send_queue {
	struct list_head msg_list;
	struct msg_status status;
	int (*put_msg)(struct msg_status * status, int skfd);
};

extern void package_message(struct message_packet * msg);
extern void message_package_head(struct message_packet * msg);
extern int put_message(struct msg_status * status, int fd);
extern int get_message(struct message_packet * msg, struct msg_status * status, int fd);

extern int send_message(struct message_send_queue * queue, int skfd);
extern struct message_packet * initial_new_input_msg(struct msg_status * status);
extern void message_parse_head(struct message_packet * msg);
extern void message_parse_body(struct message_packet * msg);
extern void present_message(struct message_packet * msg);
#endif
