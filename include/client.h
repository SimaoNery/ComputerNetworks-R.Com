#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_SIZE 256

#define SOCKET_NOT_CONNECTED    -1

#define FTP_PORT                21
#define FTP_MAX_RESPONSE_SIZE   1024

#define FTP_TRANSFER_READY1		125
#define FTP_TRANSFER_READY2		150
#define FTP_SERVER_READY		220
#define FTP_CONTROL_CLOSED		221
#define FTP_CONNECTION_CLOSED	226
#define FTP_ENTERED_PASSIVE		227
#define FTP_LOGIN_SUCCESS		230
#define FTP_PASSWORD_REQUESTED	331

typedef struct s_url {
    char user[MAX_SIZE];
    char password[MAX_SIZE];
    char host[MAX_SIZE];
    char host_name[MAX_SIZE];
    char url_path[MAX_SIZE];
    char ip[MAX_SIZE];
    char file_name[MAX_SIZE];
}	t_url;

typedef enum e_state {
    READ_CODE,
    READ_MESSAGE,
    MULTILINE_MESSAGE,
    COMPLETE
}   t_state;

int url_parse(const char *text, t_url *url);

int ftp_write(const int socket, const char* command);
int ftp_read(const int socket, char* response_buffer, int* response_code);

int ftp_connect(const char *ip, const int port, int *socket);
int ftp_close_connection(const int socket1, const int socket2);

int ftp_host_connect(const char *ip, const int port, int *socket);
int ftp_login(const int socket, const char* username, const char* password);
int ftp_enter_passive(const int socket1, char *ip, int *port);

int ftp_download_file(const int socket1, const int socket2, const char* url_path, const char* file_name);

#endif
