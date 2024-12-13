#include "client.h"

int url_parse(const char *text, t_url *url)
{
    if (url == NULL) return -1;

    int result = sscanf(text, "ftp://%255[^:]:%255[^@]@%255[^/]/%255[^\n]",
        url->user ,url->password, url->host, url->url_path);

    if (result != 4)
    {
        result = sscanf(text, "ftp://%255[^/]/%255[^\n]", url->host, url->url_path);
        
        if (result != 2) return -1;

        strcpy(url->user, "anonymous");
        strcpy(url->password, "anonymous");
    }   

    if (strlen(url->host) == 0) return -1;

    struct hostent *h = gethostbyname(url->host);
    if (h == NULL)
    {
        printf("[ERRO] Couldn't get host.");
        return -1;
    }

	struct in_addr *addr = (struct in_addr *)h->h_addr_list[0];
    strncpy(url->ip, inet_ntoa(*addr), MAX_SIZE - 1);

    strncpy(url->host_name, h->h_name, sizeof(url->host_name) - 1);
    
    char *ptr = strrchr(url->url_path, '/');
    strncpy(url->file_name, ptr == NULL ? url->url_path : ptr + 1, MAX_SIZE - 1);

    return 0;
}

int ftp_write(const int socket, const char* command)
{
	if (write(socket, command, strlen(command)) < 0)
		return -1;
    return 0;
}

int ftp_read(const int socket, char *response_buffer, int *response_code)
{
    if (response_buffer == NULL || response_code == NULL) 
        return -1;
    
    t_state state = READ_CODE;

    int index = 0;
    *response_code = 0;

    while (state != COMPLETE) {
        char byte = 0;
        int read_status = read(socket, &byte, sizeof(char));

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
	
    printf("[INFO] Message:\n - Code: %d\n - Message:\n %s\n\n", *response_code, response_buffer);
    return 0;
}

int ftp_connect(const char *ip, const int port, int *socket1)
{
    if (ip == NULL || socket1 == NULL) return -1;

    struct sockaddr_in server_addr;
	int fd;

    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[ERRO] Couldn't create socket!\n");
        return -1;
    }

    if (connect(fd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        printf("[ERRO] Couldn't connect socket!\n");
        return -1;
    }

    *socket1 = fd;

    return 0;
}

int ftp_host_connect(const char *ip, const int port, int *socket1)
{
   if (ftp_connect(ip, port, socket1)) return -1;
   
   char* response = malloc(FTP_MAX_RESPONSE_SIZE);
   int response_code = 0;

   if (ftp_read(*socket1, response, &response_code)) {
        printf("[ERRO] Couldn't read response!\n");
        free(response);
        return -1;
   }

   if(response_code != FTP_SERVER_READY) {
        printf("[ERRO] Failed connection to %s\n", ip);
        return -1;
   }

   free(response);
   return 0;
}

int ftp_enter_passive(const int socket1, char *ip, int *port)
{
    if (ip == NULL || port == NULL) return -1;

    char *command = "pasv\r\n";
    char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
    int res_code = 0;

    if (ftp_write(socket1, command)) return free(res), -1;

    if (ftp_read(socket1, res, &res_code)) return free(res), -1;

    if (res_code != FTP_ENTERED_PASSIVE) return free(res), -1;
    
    int ip2[4] = {0, 0, 0, 0}, port2[2] = {0, 0};
    
    if (sscanf(res, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", 
        ip2, ip2 + 1, ip2 + 2, ip2 + 3, port2, port2 + 1) != 6)
        return free(res), -1;
    
    snprintf(ip, MAX_SIZE, "%d.%d.%d.%d", ip2[0], ip2[1], ip2[2], ip2[3]);

    *port = (port2[0] << 8) + port2[1];

    printf("[INFO] Socket2\n - IP: %s\n - Port: %d\n", ip, *port);
    return free(res), 0;
}

int ftp_login(const int socket, const char* username, const char* password) {
    if (username == NULL || password == NULL) return -1;

    char *command = malloc(FTP_MAX_RESPONSE_SIZE);
    char *response = malloc(FTP_MAX_RESPONSE_SIZE);
    int response_code = 0;
    
    snprintf(command, FTP_MAX_RESPONSE_SIZE, "USER %s\r\n", username);
    if (ftp_write(socket, command))
        return free(command), free(response), -1;
    
    if (ftp_read(socket, response, &response_code))
        return free(command), free(response), -1;
    
    if (response_code != FTP_PASSWORD_REQUESTED) {
        printf("[ERRO] Login failed with user: %s\n", username);
        free(command), free(response);
        return -1;    
    }
    
    snprintf(command, FTP_MAX_RESPONSE_SIZE, "PASS %s\r\n", password);
    
    if (ftp_write(socket, command))
        return free(command), free(response), -1;
    
    if (ftp_read(socket, response, &response_code))
        return free(command), free(response), -1;

    if (response_code != 230) {
        printf("[ERRO] Login failed with password: %s\n", password);
        free(command), free(response);
        return -1;
    }

    return free(command), free(response), 0;
}

// download
int ftp_download_file(const int socket1, const int socket2, const char* url_path, const char* file_name) {
    char *command = malloc(FTP_MAX_RESPONSE_SIZE);
    char *response = malloc(FTP_MAX_RESPONSE_SIZE);
    int response_code = 0;

    snprintf(command, FTP_MAX_RESPONSE_SIZE, "retr %s\r\n", url_path);
    
    if (ftp_write(socket1, command))
        return free(command), free(response), -1;

    if (ftp_read(socket1, response, &response_code))
        return free(command), free(response), -1;

    if (response_code / 100 != 1) {
        printf("[ERRO] Invalid code (URL path: %s)\n", url_path);
        free(command), free(response);
        return -1;
    }

	int total_file = 1;
	sscanf(response, "Opening %255[^ ] mode data connection for %255[^ ] (%d bytes)", response, response, &total_file);

    FILE *file = fopen(file_name, "wb");

    if (file == NULL) {
        printf("[ERRO] Couldn't open file\n\n");
        return -1;
    }

    char *buffer = malloc(MAX_SIZE);
    int bytes_read = read(socket2, buffer, MAX_SIZE);

    int total = 0;
    while (bytes_read > 0)
	{
        if (write(file->_fileno, buffer, bytes_read) < 0)
            return free(buffer), free(response), free(command), -1;
        total += bytes_read;

		float perc = (total * 1.0f / total_file) * 100;
        printf("\r[INFO] Current progress: %d bytes (%.2f%%)", total, perc);
		fflush(stdout);
        bytes_read = read(socket2, buffer, MAX_SIZE);
    }

	printf("\n");

    fclose(file);

    while (1) {
        if (ftp_read(socket1, response, &response_code) < 0) {
            fprintf(stderr, "[ERRO] Failed to read FTP response.\n");
            free(buffer), free(response), free(command);
            return -1;
        }

        if (response_code != FTP_CONNECTION_CLOSED) {
            fprintf(stderr, "[ERRO] Transfer was not complete. Expected code: %d, but got: %d\n", 226, response_code);
            free(buffer), free(response), free(command);
            return -1;
        }

        if (response_code / 100 >= 2) break;
    }

    free(buffer), free(response), free(command);
    return EXIT_SUCCESS;
}

int ftp_close_connection(const int socket1, const int socket2)
{
    char *command = "quit\r\n";
    char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
    int res_code = 0;
    
    if (ftp_write(socket1, command)) return free(res), -1;
    
    if (ftp_read(socket1, res, &res_code)) return free(res), -1;

    if (res_code != FTP_CONTROL_CLOSED)
    {
        printf("[ERRO] Couldn't close connection [CODE %d]\n", res_code);
        return free(res), -1;
    }
    
    if (close(socket1) < 0) return -1;

    if (socket2 >= 0 && close(socket2) < 0) return -1;

    return 0;
}
