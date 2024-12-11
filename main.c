#include "client.h"

int main(int argc, char *argv[])
{
    t_url url;
    bzero(&url, sizeof(t_url));

    if (argc != 2)
    {
        printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path> \n", argv[0]);
        exit(-1);
    }

    // Parse URL
    
    if (url_parse(argv[1], &url))
    {
        printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    printf("[INFO] Parsed details from URL '%s'\n"
           "- User: %s \n"
           "- Password: %s\n"
           "- Host: %s\n" 
           "- Host name: %s\n"
           "- URL path: %s\n"
           "- IP: %s\n",
           argv[1],
           url.user,
           url.password,
           url.host,
           url.host_name,
           url.url_path,
           url.ip);

    // Establish Connection

    int socket1, socket2;

    if (ftp_host_connect(url.ip, FTP_PORT, &socket1))
        exit(-1);
    
    printf("[INFO] Socket1: %d\n", socket1);

    // Login with credentials or Anonimous Mode
    if (ftp_login(socket1, url.user, url.password))
    {
        ftp_close_connection(socket1, SOCKET_NOT_CONNECTED);
        exit(-1);
    }

    // Activate Passive Mode
    
    char* data_ip = malloc(256); // ip tp connect socket2

    int data_port = 0;
    
    if (ftp_enter_passive(socket1, data_ip, &data_port))
    {
        ftp_close_connection(socket1, SOCKET_NOT_CONNECTED);
        exit(-1);
    }

    // Download the File
    if (ftp_connect(data_ip, data_port, &socket2))
    {
        ftp_close_connection(socket1, SOCKET_NOT_CONNECTED);
        exit(-1);
    }

    printf("[INFO] Socket2: %d\n", socket2);
    
    if (ftp_download_file(socket1, socket2, url.url_path, url.file_name))
    {
        ftp_close_connection(socket1, socket2);
        exit(-1);
    }

    if (ftp_close_connection(socket1, socket2)) exit(-1);

    return 0;
}
