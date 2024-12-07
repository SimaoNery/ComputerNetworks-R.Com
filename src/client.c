#include "client.h"

//ftp://[<user>:<password>@]<host>/<url-path>
int parse_url(const char *text, t_url *url)
{
    if (url == NULL) return -1;

    int result = sscanf(text, "ftp://%255[^:]:%255[^@]@%255[^/]/%255[^\n]",
        url->user ,url->password, url->host, url->url_path);

    if (result != 4)
    {
        result = sscanf(text, "ftp://%255[^/]/%255[^\n]", url->host, url->url_path);
        
        if (result != 2) return -1;

        strncpy(url->user, "anonymous", MAX_SIZE - 1);
        strncpy(url->password, "anonymous", MAX_SIZE - 1);
    }   

    if (strlen(url->host) == 0) return -1;

    struct hostent *h = gethostbyname(url->host);
    if (h == NULL)
    {
        printf("Error getting host.");
        return -1;
    }

    strncpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr_list[0])), MAX_SIZE - 1);

    strncpy(url->host_name, h->h_name, sizeof(url->host_name) - 1);
    
    char *last_slash = strrchr(url->url_path, '/');
    strncpy(url->filename, last_slash == NULL ? url->url_path : last_slash + 1, MAX_SIZE - 1);

    return 0;
}

int send_ftp_command(const int socket, const char* command) {
    size_t bytes = write(socket, command, strlen(command));
    if(bytes < 0) return -1;

    return EXIT_SUCCESS;
}

int read_ftp_response(const int socket, char* response_buffer, int* response_code) {
    if (response_buffer == NULL || response_code == NULL) 
        return -1;
    
    t_state state = READ_CODE;

    int index = 0;
    *response_code = 0;
    int previous_code = 0;

    while (state != COMPLETE) {
        char byte = 0;
        int read_status = read(socket, &byte, sizeof(byte));

        if(read_status == -1) return -1;

        switch (state) {
            case READ_CODE:
                if (byte >= '0' && byte <= '9') {
                    *response_code = *response_code * 10 + (byte - '0'); 
                }
                else if (byte == ' ')  {
                    state = READ_MESSAGE;
                }
                else if (byte == '-') {
                    state = MULTILINE_MESSAGE;
                    previous_code = *response_code;
                }
                break;
            case READ_MESSAGE:
                if (byte == '\n') {
                    response_buffer[index] = '\0';
                    state = COMPLETE;
                } else {
                    response_buffer[index++] = byte;
                    if (index >= FTP_MAX_RESPONSE_SIZE - 1) {
                        response_buffer[index] = '\0';
                        return -1;
                    }
                }
                break;
            case MULTILINE_MESSAGE:
                response_buffer[index++] = byte;
                if(index >= FTP_MAX_RESPONSE_SIZE - 1) {
                    response_buffer[index] = '\0';
                    return -1;
                }

                if (byte == '\n') {
                    state = READ_CODE;
                    *response_code = 0;
                }
                break;
            case COMPLETE:
                break;
            default:
                return -1;
        } 
    }
    printf("[%d] Message: \n %s\n\n", *response_code, response_buffer);
}

int connect_socket(const char *ip, const int port, int *socket_fd)
{
    if(ip == NULL || socket_fd == NULL) return -1;

    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

     /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        return -1;
    }

    *socket_fd = sockfd;

    return 0 ;
}

int establish_connection(const char *ip, const int port, int *socket_fd)
{
   if(connect_socket(ip, port, socket_fd)) return -1;
   
   char* response = malloc(FTP_MAX_RESPONSE_SIZE);
   int response_code = 0;

   if(read_ftp_response((*socket_fd), response, &response_code)) {
        printf("Error while trying to read response!\n");
        free(response);
        return -1;
   }

   if(response_code != 220) {
        printf("Failed connection to %s\n", ip);
        return -1;
   }

   free(response);
   return 0;
}

int enter_ftp_passive_mode(const int socket1, char *ip, int *port)
{
    if (ip == NULL || port == NULL) return -1;

    char *command = "pasv\r\n";
    char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
    int res_code = 0;

    if (send_ftp_command(socket1, res)) return free(res), -1;

    if (read_ftp_response(socket1, res, &res_code)) return free(res), -1;

    if (res_code != 227) free(res), -1;
    
    int ip[4], pt[2];
    
    if (sscanf(res, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", 
        ip[0], ip[1], ip[2], ip[3], pt[0], pt[1]) != 6)
        return freee(res), -1;
    
    snprintf(ip, MAX_SIZE, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    *port = (pt[1] << 8) + pt[1];
    
    return free(res), 0;
}

// login
int login_ftp(const int socket, const char* username, const char* password) {
    if (username == NULL || password == NULL) return -1;

    char *command = malloc(FTP_MAX_RESPONSE_SIZE);
    char *response = malloc(FTP_MAX_RESPONSE_SIZE);
    int response_code = 0;
    
    snprintf(command, FTP_MAX_RESPONSE_SIZE, "USER %s\r\n", username);
    if (send_ftp_command(socket, command)) {
        free(command), free(response);
        return -1;
    }
    
    if (read_ftp_response(socket, response, &response_code)) {
        free(command), free(response);
        return -1;
    }
    
    if (response_code != 331) {
        printf("login failed with user: %s\n", username);
        free(command), free(response);
        return -1;    
    }
    
    snprintf(command, FTP_MAX_RESPONSE_SIZE, "PASS %s\r\n", password);
    
    if (send_ftp_command(socket, command))
    {
        free(command), free(response);
        return -1;
    
    }
    
    if (read_ftp_response(socket, response, &response_code)) {
        free(command), free(response);
        return -1;
    }

    if (response_code != 230) {
        printf("login failed with password: %s\n", password);
        free(command), free(response);
        return -1;
    }

    return free(command), free(response), 0;
}

// download
int download_file(const int socket1, const int socket2, const char* url_path, const char* filename) {
    char *command = malloc(FTP_MAX_RESPONSE_SIZE);
    char *response = malloc(FTP_MAX_RESPONSE_SIZE);
    int response_code = 0;

    snprintf(command, FTP_MAX_RESPONSE_SIZE, "retr %s\r\n", url_path);
    
    if (send_ftp_command(socket, command)) {
        free(command), free(response);
        return -1;
    }

    if (read_ftp_response(socket, response, &response_code)) {
        free(command), free(response);
        return -1;
    }

    if (response_code != 100) {
        printf("retr command with URL path: %s\n", url_path);
        free(command), free(response);
        return -1;
    }

    FILE *file = fopen(filename, "wb");

    if (file == NULL) {
        printf("error while opening file\n");
        return -1;
    }

    char *buffer = malloc(MAX_SIZE);
    int bytes_read = read(socket2, buffer, MAX_SIZE);

    while(bytes_read > 0) {
        if(fwrite(buffer, bytes_read, 1, file) < 0) {
            free(buffer), free(response), free(command);
            return -1;
        }
    }

    fclose(file);

    while (1) {
        if(read_ftp_response(socket1, response, &response_code) < 0) {
            fprintf(stderr, "Failed to read FTP response.\n");
            free(buffer), free(response), free(command);
            return -1;
        }

        if (response_code != 226) {
            fprintf(stderr, "Transfer was not complete. Expected code: %d, but got: %d\n", 226, response_code);
            free(buffer), free(response), free(command);
            return -1;
        }

        if (response_code / 100 >= 2) break;
    }

    free(buffer), free(response), free(command);
    return EXIT_SUCCESS;
}

int close_connection(const int socket1, const int socket2)
{
    char *command = "quit\r\n";
    char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
    int res_code = 0;
    
    if (send_ftp_command(socket1, command)) return free(res), -1;
    
    if (read_ftp_response(socket1, res, &res_code)) return free(res), -1;

    if (res_code != 221)
    {
        printf("Error trying to close connection [CODE %d]\n", res_code);
        return free(res), -1;
    }
    
    if (close(socket1) < 0) return -1;
    if (socket2 >= 0 && close(socket2) < 0) return -1;

    return 0;
}
