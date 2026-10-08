#ifndef FORWARDER_H
#define FORWARDER_H

#include <winsock2.h>
#include "http_parser.h"

int forward_http_request(
    SOCKET client_socket,
    const char *original_request,
    const HttpRequest *parsed_request
);

#endif