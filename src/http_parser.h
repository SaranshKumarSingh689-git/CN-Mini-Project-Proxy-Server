#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define MAX_METHOD_LENGTH 16
#define MAX_HOST_LENGTH 256
#define MAX_PATH_LENGTH 2048
#define MAX_HTTP_REQUEST 8192

typedef struct
{
    char method[MAX_METHOD_LENGTH];
    char host[MAX_HOST_LENGTH];
    int port;
    char path[MAX_PATH_LENGTH];
} HttpRequest;

int parse_http_request(
    const char *request,
    HttpRequest *parsed_request
);

#endif