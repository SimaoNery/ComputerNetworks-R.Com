#include "client.h"

int url_parse(const char *text, t_url *url)
{
	if (url == NULL)
		return -1;

	int result = sscanf(text, "ftp://%255[^:]:%255[^@]@%255[^/]/%255[^\n]",
						url->user, url->password, url->host, url->url_path);

	if (result != 4)
	{
		result = sscanf(text, "ftp://%255[^/]/%255[^\n]", url->host, url->url_path);

		if (result != 2)
			return -1;

		strcpy(url->user, "anonymous");
		strcpy(url->password, "anonymous");
	}

	if (strlen(url->host) == 0)
		return -1;

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
	url->file_name[MAX_SIZE - 1] = '\0';

	return 0;
}

int ftp_write(const int socket, const char *command)
{
	printf("\n[INFO] Write\n%s", command);
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

	while (state != COMPLETE)
	{
		char byte = 0;
		int read_status = read(socket, &byte, sizeof(char));

		if (read_status == -1)
			return -1;

		response_buffer[index++] = byte;
		response_buffer[index] = '\0';

		if (index > FTP_MAX_RESPONSE_SIZE - 1)
			return -1;

		if (byte != '\n')
			continue;

		int code = atoi(response_buffer);
		if (index >= 4 && code >= 100 && code <= 999 && response_buffer[3] == ' ')
		{
			sscanf(response_buffer, "%d", response_code);
			break;
		}
		
		index = 0;
	}

	printf("\n[INFO] Read [%d]\n%s", *response_code, response_buffer);
	return 0;
}

int ftp_connect(const char *ip, const int port, int *socket1)
{
	if (ip == NULL || socket1 == NULL)
		return -1;

	struct sockaddr_in server_addr;
	int fd;

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("[ERRO] Couldn't create socket!\n");
		return -1;
	}

	if (connect(fd,
				(struct sockaddr *)&server_addr,
				sizeof(server_addr)) < 0)
	{
		printf("[ERRO] Couldn't connect socket!\n");
		return -1;
	}

	*socket1 = fd;

	return 0;
}

int ftp_host_connect(const char *ip, const int port, int *socket1)
{
	if (ftp_connect(ip, port, socket1))
		return -1;

	char *response = malloc(FTP_MAX_RESPONSE_SIZE);
	int response_code = 0;

	if (ftp_read(*socket1, response, &response_code))
	{
		printf("[ERRO] Couldn't read response!\n");
		free(response);
		return -1;
	}

	if (response_code != FTP_SERVER_READY)
	{
		printf("[ERRO] Failed connection to %s\n", ip);
		return -1;
	}

	free(response);
	return 0;
}

int ftp_enter_passive(const int socket1, char *ip, int *port)
{
	if (ip == NULL || port == NULL)
		return -1;

	char *command = "pasv\r\n";
	char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
	int res_code = 0;

	if (ftp_write(socket1, command))
		return free(res), -1;

	if (ftp_read(socket1, res, &res_code))
		return free(res), -1;

	if (res_code != FTP_ENTERED_PASSIVE)
		return free(res), -1;

	int ip2[4] = {0, 0, 0, 0}, port2[2] = {0, 0};

	if (sscanf(res, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
			   ip2, ip2 + 1, ip2 + 2, ip2 + 3, port2, port2 + 1) != 6)
		return free(res), -1;

	snprintf(ip, MAX_SIZE, "%d.%d.%d.%d", ip2[0], ip2[1], ip2[2], ip2[3]);

	*port = (port2[0] << 8) + port2[1];

	printf("[INFO] Socket2\n - IP: %s\n - Port: %d\n", ip, *port);
	return free(res), 0;
}

int ftp_login(const int socket, const char *username, const char *password)
{
	if (username == NULL || password == NULL)
		return -1;

	char *command = malloc(FTP_MAX_RESPONSE_SIZE);
	char *response = malloc(FTP_MAX_RESPONSE_SIZE);
	int response_code = 0;

	snprintf(command, FTP_MAX_RESPONSE_SIZE, "USER %s\r\n", username);
	if (ftp_write(socket, command))
		return free(command), free(response), -1;

	if (ftp_read(socket, response, &response_code))
		return free(command), free(response), -1;

	if (response_code != FTP_PASSWORD_REQUESTED)
	{
		printf("[ERRO] Login failed with user: %s\n", username);
		free(command), free(response);
		return -1;
	}

	snprintf(command, FTP_MAX_RESPONSE_SIZE, "PASS %s\r\n", password);

	if (ftp_write(socket, command))
		return free(command), free(response), -1;

	if (ftp_read(socket, response, &response_code))
		return free(command), free(response), -1;

	if (response_code != 230)
	{
		printf("[ERRO] Login failed with password: %s\n", password);
		free(command), free(response);
		return -1;
	}

	return free(command), free(response), 0;
}

// download
int ftp_download_file(const int socket1, const int socket2, const char *url_path, const char *file_name)
{
	char *command = malloc(FTP_MAX_RESPONSE_SIZE);
	char *response = malloc(FTP_MAX_RESPONSE_SIZE);
	int response_code = 0;

	snprintf(command, FTP_MAX_RESPONSE_SIZE, "retr %s\r\n", url_path);

	if (ftp_write(socket1, command))
		return free(command), free(response), -1;

	if (ftp_read(socket1, response, &response_code))
		return free(command), free(response), -1;

	if (response_code != FTP_TRANSFER_READY1 && response_code != FTP_TRANSFER_READY2)
	{
		printf("[ERRO] Invalid code (URL path: %s)\n", url_path);
		free(command), free(response);
		return -1;
	}

	FILE *file = fopen(file_name, "wb");

	if (file == NULL)
	{
		printf("[ERRO] Couldn't open file\n\n");
		return -1;
	}

	char *buffer = malloc(FTP_MAX_RESPONSE_SIZE);
	int bytes_read = read(socket2, buffer, FTP_MAX_RESPONSE_SIZE);

	while (bytes_read > 0)
	{
		if (write(file->_fileno, buffer, bytes_read) < 0)
			return free(buffer), free(response), free(command), -1;

		bytes_read = read(socket2, buffer, FTP_MAX_RESPONSE_SIZE);
	}

	fclose(file);
	close(socket2);

	if (ftp_read(socket1, response, &response_code) < 0)
	{
		fprintf(stderr, "[ERRO] Failed to read FTP response.\n");
		free(buffer), free(response), free(command);
		return -1;
	}

	if (response_code != FTP_CONNECTION_CLOSED)
	{
		fprintf(stderr, "[ERRO] Transfer was not complete. Expected code: %d, but got: %d\n",
				FTP_CONNECTION_CLOSED, response_code);
		free(buffer), free(response), free(command);
		return -1;
	}

	free(buffer), free(response), free(command);
	return EXIT_SUCCESS;
}

int ftp_close_connection(const int socket1, const int socket2)
{
	char *command = "quit\r\n";
	char *res = calloc(FTP_MAX_RESPONSE_SIZE, sizeof(char));
	int res_code = 0;

	if (ftp_write(socket1, command))
		return free(res), -1;

	if (ftp_read(socket1, res, &res_code))
		return free(res), -1;

	if (res_code != FTP_CONTROL_CLOSED)
	{
		printf("[ERRO] Couldn't close connection [CODE %d]\n", res_code);
		return free(res), -1;
	}

	close(socket1);
	close(socket2);

	return 0;
}
