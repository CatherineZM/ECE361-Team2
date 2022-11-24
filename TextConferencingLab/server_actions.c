#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>

#include "message.h"
#include "server_actions.h"

//global info
extern char online_users[USERNO][MAX_NAME];
extern int session_list[SESSIONNO];
extern char session_names[SESSIONNO][MAX_NAME];
extern char session_members[SESSIONNO*USERNO][MAX_NAME];
extern int session_fd[SESSIONNO*USERNO];

//initialize global variable
void initialize() {
	for(int i=0; i<USERNO; i++) {
		strcpy(online_users[i], "EMPTY");
	}
	for(int i=0; i<SESSIONNO; i++) {
		session_list[i] = 0;
	}
	for(int i=0; i<SESSIONNO; i++) {
		strcpy(session_names[i], "EMPTY");
	}
	for(int i=0; i<SESSIONNO*USERNO; i++) {
		strcpy(session_members[i], "EMPTY");
		session_fd[i] = -1;
	}
	for(int i=0; i<SESSIONNO*USERNO; i++) {
		strcpy(session_members[i], "EMPTY");
	}
}

void get_online_list() {
	for(int i=0; i<USERNO; i++) {
		printf("online users:\n%s\n", online_users[i]);
	}
	for(int i=0; i<SESSIONNO; i++) {
		printf("session list:\n%d %d\n",i, session_list[i]);
	}
	for(int i=0; i<SESSIONNO; i++) {
		printf("session names:\n%s\n",session_names[i]);
	}
	for(int i=0; i<SESSIONNO*USERNO; i++) {
		printf("session:\n%s\n", session_members[i]);
	}
}


//fork child to solo with the client
void exclusive_service(int socketfd, int client_sock) {
	//int pid = fork();
	//error_check(pid, NONNEGATIVE, "fork");
	/*if(pid) {
		error_check(close(client_sock), ZERO, "close");
		return 0;
	}
	*/
	//child starts here
	error_check(close(socketfd), ZERO, "close");
	while(1) {
	//clean buffers
	char server_message[MSGBUFLEN], client_message[MSGBUFLEN]; int numbytes;
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));
	//recv client msg
	numbytes = recv(client_sock, client_message, sizeof(client_message), 0);
	error_check(numbytes, NONNEGATIVEONE, "recv");
	printf("Received a client request\n");
	client_message[numbytes] = '\0';
	struct message client_message_struct, server_message_struct;
	memset(&client_message_struct, 0, sizeof client_message_struct);
	memset(&server_message_struct, 0, sizeof server_message_struct);
	printf("Reading the message...\n");
	sort_message(client_message, &client_message_struct);
	printf("Detecting the action...\n");
	int reply = action_detect(&client_message_struct, &server_message_struct, client_sock);
	error_check(reply, NONNEGATIVEONE, "action_detect");
	if(reply == 2) {
		printf("User logged out\n\n");
		error_check(close(client_sock), ZERO, "close");
		exit(0);
	}
	if(reply) {
		printf("Making response message...\n");
		make_message(server_message, &server_message_struct);
		error_check(send(client_sock, server_message, strlen(server_message), 0), NONNEGATIVEONE, "send");
		printf("Server sent response\n");
	}
	printf("\n");
	}
}

//make server message string

void make_message(char server_message[MSGBUFLEN], struct message* server_message_struct) {
	sprintf(server_message, "%d:%d:%s:%s", server_message_struct->type, server_message_struct->size, 
	server_message_struct->source, server_message_struct->data);
	printf("User chating message \"%s\"\n", server_message);
}


//detect client action
int action_detect(struct message* client_message_struct, struct message* server_message_struct, int client_sock) {
	enum command type;
	type = client_message_struct->type;
	switch(type) {
		case LOGIN:
			printf("LOGIN request detected\n");
			login(client_message_struct, server_message_struct);
			//get_online_list();
			return 1;
			break;
		case EXIT:
			printf("EXIT request detected\n");
			update_list(client_message_struct, server_message_struct, 1);
			return 2;
			exit(1);
			//get_online_list();
			break;
		case JOIN:
			printf("JOIN request detected\n");
			join(client_message_struct, server_message_struct, client_sock);
			//get_online_list();
			return 1;
			break;
		case LEAVE_SESS:
			printf("LEAVE_SESS request detected\n");
			update_list(client_message_struct, server_message_struct, 0, client_sock);
			//get_online_list();
			break;
		case NEW_SESS:
			printf("NEW_SESS request detected\n");
			new_sess(client_message_struct, server_message_struct, client_sock);
			//get_online_list();
			return 1;
			break;
		case MESSAGE:
			printf("MESSAGE request detected\n");
			message(client_message_struct, server_message_struct);
			//return 1;
			break;
		case QUERY:
			printf("QUERY request detected\n");
			query(client_message_struct, server_message_struct);
			return 1;
			break;
      	default:
			printf("Fail to detect client's action\n" );
			return -1;
	}
	printf("Action detection finished\n");
	return 0;
}

//communciation between users
void message(struct message* client_message_struct, struct message* server_message_struct) {
	char temp[MAX_DATA]; int fds = 0;
	char source[MAX_DATA]; int sid; char server_message[MAX_DATA];
	strcpy(source, client_message_struct->source);
	for(int i=0; i<USERNO*SESSIONNO; i++) {
		if(!strcmp(strcmp(session_members[i], source)) {
			sid = i/USERNO;
			break;
		}
	}
	for(int i=0; i<USERNO*SESSIONNO; i+=3) {
		if(session_fd[i] != -1) {
			int fd = session_fd[i];
			set_msg_struct(MESSAGE, server_message_struct->size, source, client_message_struct->date, server_message_struct);
			make_message(server_message, &server_message_struct);
			memset(server_message, '\0', sizeof(server_message));
			error_check(send(fd, server_message, strlen(server_message), 0), NONNEGATIVEONE, "send");
		}
	}
}

//get active info
void query(struct message* client_message_struct, struct message* server_message_struct) {
	printf("Loading active users and sessions...\n");
	//char separate[] = " / ";
	//char online_names[MAX_DATA];
	//char online_sessions[MAX_DATA];
	char reply[MAX_DATA] = "ACTIVES ";
	for(int i=0; i<USERNO*SESSIONNO; i++) {
		if(i%3 == 0) {
			if(strcmp(session_members[i], "EMPTY")) {
				strcat(reply, session_names[i/3]);
				strcat(reply, " - ");
				for(int j=0; j<USERNO; j++) {
					if(strcmp(session_members[i+j], "EMPTY")) {
						strcat(reply, session_members[i+j]);
						strcat(reply, ",");
					}
				}	
			}
		}
	}
	int count=0;
	for(int i=0; i<SESSIONNO; i++) {
		if(!strcmp(session_names[i], "EMPTY")) {
			count++;
		}
	}
	if(count == SESSIONNO) {
		strcpy(reply, "NO ACTIVEs");
	}
	/*
	strcpy(online_names, "Active Users - ");
	for(int i=0; i<USERNO; i++) {
		if(strcmp(online_users[i], "EMPTY")) {
			strcat(online_names, online_users[i]);
			strcat(online_names, separate);
		}
	}
	strcpy(online_sessions, "Active Sessions - ");
	for(int i=0; i<SESSIONNO; i++) {
		if(strcmp(session_names[i], "EMPTY")) {
			strcat(online_sessions, session_names[i]);
			strcat(online_sessions, separate);
		}
	}
	strcat(online_names, online_sessions);
	*/
	printf("Completed making active info\n");
	printf("server message \"%s\"\n", reply);
	set_msg_struct(QU_ACK, strlen(reply), "Server", reply, server_message_struct);
}

//find valid session
void new_sess(struct message* client_message_struct, struct message* server_message_struct) {
	printf("Creating a new session...\n");
	int sid = -1;
	for(int i=0; i<SESSIONNO; i++) {
		if(session_list[i] == 0) {
			session_list[i] = 1;
			sid = i;
			break;
		}
	}
	if(sid<0) {
		printf("error: can not find valid session space\n");
	}
	strcpy(session_names[sid], client_message_struct->data);
	make_message(server_message, &server_message_struct);
		error_check(send(client_sock, server_message, strlen(server_message), 0), NONNEGATIVEONE, "send");
	printf("New session created:%s at %d\n", client_message_struct->data,sid);

	for(int i=(sid)*USERNO; i<(sid+1)*USERNO; i++) {
		if(!strcmp(session_members[i], "EMPTY")) {
			strcpy(session_members[i], client_message_struct->source);
			break;
		}
	}
	set_msg_struct(NS_ACK, client_message_struct->size, "Server", client_message_struct->data, server_message_struct);
}


//exit and leave function
void update_list(struct message* client_message_struct, struct message* server_message_struct, int all) {
	printf("Updating list info\n");
	char source[MAX_DATA]; int count; int sid;
	strcpy(source, client_message_struct->source);
	if(all) {
		for(int i=0; i<USERNO; i++) {
			if(!strcmp(source, online_users[i])) {
				strcpy(online_users[i], "EMPTY");
				break;
			}
		}
	}
	for(int i=0; i<SESSIONNO*USERNO; i++) {
		if(i%USERNO == 0) {
			count = 0;
		}
		if(!strcmp(source, session_members[i])) {
			strcpy(session_members[i], "EMPTY");
		}
		count++;
		if(count == USERNO) {
			sid = i/USERNO;
			session_list[sid] = 0;
			strcpy(session_names[sid], "EMPTY");
		}
	}
	printf("List updated\n");
}


//join function
void join(struct message* client_message_struct, struct message* server_message_struct) {
	char id[MAX_DATA];
	char session[MAX_DATA];
	strcpy(id, client_message_struct->source);
	strcpy(session, client_message_struct->data);
	char reply[MAX_DATA];

	if(!loggedin(id)) {
		strcpy(reply, session);
		strcat(reply, ", [ERROR] user should login first");
		set_msg_struct(JN_NAK, strlen(reply), "Server", reply, server_message_struct);
		return;
	}
	if(!session_opened(atoi(session))) {
		strcpy(reply, session);
		strcat(reply, ", [ERROR] session does not exist");
		set_msg_struct(JN_NAK, strlen(reply), "Server", reply, server_message_struct);
		return;
	}
	int session_id = atoi(session);
	for(int i=(session_id)*USERNO; i<(session_id+1)*USERNO; i++) {
		if(!strcmp(session_members[i], "EMPTY")) {
			strcpy(session_members[i], id);
			strcpy(reply, session);
			strcat(reply, ", [SUCCESS] user joined session successfully");
			set_msg_struct(JN_ACK, strlen(reply), "Server", reply, server_message_struct);
			return;
		}
	}
	printf("error: session full\n");
}

//if session open
bool session_opened(int session) {
	if(session<0 || session>=SESSIONNO) {
		return false;
	}
	if(session_list[session]) {
		return true;
	}
	return false;
}

//login function
void login(struct message* client_message_struct, struct message* server_message_struct) {
	//set id and psw
	char id[MAX_DATA];
	char psw[MAX_DATA];
	char reply[MAX_DATA];
	strcpy(id, client_message_struct->source);
	strcpy(psw, client_message_struct->data);

	char users[USERNO*2][MAX_DATA] = {
                   					"cardiA",
                   					"123",
                   					"cat",
									"meow",
                   					"anonymous",
                   					"uoftECF"
								};
	printf("Start matching user ID and password\n");
	//matching user id and psw
	bool match = false;
	for(int i=0; i<USERNO*2; i+=2) {
		if(!strcmp(id, users[i])) {
			if(!strcmp(psw, users[i+1])) {
				match = true;
			}
			break;
		}
	}
	if(!match) {
		strcpy(reply, "[ERROR] user ID or password is incorrect");
		set_msg_struct(LO_NAK, strlen(reply), "Server", reply, server_message_struct);
		return;
	}
	//checking existing user
	if(loggedin(id)) {
		strcpy(reply, "[ERROR] user already logged in");
		set_msg_struct(LO_NAK, strlen(reply), "Server", reply, server_message_struct);
		return;
	}
	//good login
	//get_online_list();
	for(int i=0; i<USERNO; i++) {
		if(!strcmp(online_users[i], "EMPTY")) {
			strcpy(online_users[i], id);
			strcpy(reply, "[SUCCESS] user logged in successfully ");
			set_msg_struct(LO_ACK, strlen(reply), "Server", reply, server_message_struct);
			return;
		}
	}
	printf("failed to login user, server full\n");
	return;
}

//checking if user logged in
bool loggedin(char id[MAX_NAME]) {
	for(int i=0; i<USERNO; i++) {
		if(!strcmp(id, online_users[i])) {
			return true;
		}
	}
	return false;
}

//setting server message struct
void set_msg_struct(int type, int size, char source[MAX_NAME], char data[MAX_DATA], struct message* server_message_struct) {
	server_message_struct->type = type;
	server_message_struct->size = size;
	strcpy(server_message_struct->source, source);
	strcpy(server_message_struct->data, data);
}

//sort client message string into message struct form
void sort_message(char client_message[MSGBUFLEN], struct message* client_message_struct) {
	int count = 0, len = 0;
	char *token = NULL;
	const char separate[] = ":";
	char msg_copy[MSGBUFLEN];
	printf("client message string \"%s\"\n", client_message);
	strcpy(msg_copy, client_message);
	token = strtok(msg_copy, separate);
	while(token != NULL && count < 3) {
		len += strlen(token);
		count++;
		if(count == 1) {
			client_message_struct->type = atoi(token);
		}
		if(count == 2) {
			client_message_struct->size = atoi(token);
		}
		if(count == 3) {
			strcpy(client_message_struct->source, token);
		}
     	token = strtok(NULL, separate);
   	}
	strncpy(client_message_struct->data, client_message + len + 3, client_message_struct->size);
	printf("Message sorted\n");
	printf("type = %d, size = %d, source = %s, data = %s\n", client_message_struct->type, 
	client_message_struct->size, client_message_struct->source, client_message_struct->data);
}

//error check
void error_check(int ret, int suc, const char *msg) {
	switch(suc) {
		case NONZERO:
			if(ret != 0) {
				return;
			}
			break;
		case ZERO:
			if(ret == 0) {
				return;
			}
			break;
		case NONNEGATIVE:
			if(ret >= 0) {
				return;
			}
			break;
		case NONNEGATIVEONE:
			if(ret != -1) {
				return;
			}
			break;
      	default:
			printf("Invalid assertion\n" );
	}
	int err = errno;
    perror(msg);
    exit(err);
}

//check valid input
void input_check(int argc, char *argv[]) {
	if(argc != 2 || !numeric(argv)) {
		printf("Invalid input, command input should be as follow:\nserver <TCP port number to listen on>\n");
		exit(-1);
	}
}

//check numeric
bool numeric(char *argv[]) {
	for(int i=0, n=strlen(argv[1]); i<n; i++) {
        if (!isdigit(argv[1][i])) {
            return false;
        }
    }
    return true;
}