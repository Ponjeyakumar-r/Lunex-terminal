#ifndef SHELL_WEB_H
#define SHELL_WEB_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>

#ifndef WEB_PORT
#define WEB_PORT (getenv("PORT") ? atoi(getenv("PORT")) : 8080)
#endif

void start_web_server(void);
void handle_web_client(int client_fd);
void send_terminal_html(int client_fd);

int create_server_socket(int port);
char *parse_http_request(char *buffer, char *path, char *query, char *method_out);
void url_decode(char *str);

#endif