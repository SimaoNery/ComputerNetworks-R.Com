#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_SIZE 256

#define SOCKET_NOT_CONNECTED    -1

#define FTP_PORT                21
#define FTP_MAX_RESPONSE_SIZE   1024 

typedef struct s_url {
    char user[MAX_SIZE];
    char password[MAX_SIZE];
    char host[MAX_SIZE];
    char host_name[MAX_SIZE];
    char url_path[MAX_SIZE];
    char ip[MAX_SIZE];
    char filename[MAX_SIZE];
}	t_url;

typedef enum e_state {
    READ_CODE,
    READ_MESSAGE,
    MULTILINE_MESSAGE,
    COMPLETE
}   t_state;

int parse_url(const char *text, t_url *url);
int send_ftp_command(const int socket, const char* command);
int read_ftp_response(const int socket, char* response_buffer, int* response_code);
int connect_socket(const char *ip, const int port, int *socket_fd);
int establish_connection(const char *ip, const int port, int *socket_fd);
int enter_ftp_passive_mode(const int socket1, char *ip, int *port);
int login_ftp(const int socket, const char* username, const char* password);
int download_file(const int socket1, const int socket2, const char* url_path, const char* filename);
int close_connection(const int socket1, const int socket2);

#endif
