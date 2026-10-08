#ifndef CACHE_H
#define CACHE_H

#include <winsock2.h>

#define CACHE_TTL_SECONDS 60

int cache_init(void);

int cache_get(
    const char *host,
    int port,
    const char *path,
    SOCKET client_socket
);

int cache_store(
    const char *host,
    int port,
    const char *path,
    const char *response,
    int response_length
);

#endif