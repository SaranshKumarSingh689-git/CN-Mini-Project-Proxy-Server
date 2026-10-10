#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "access_control.h"
#include "http_parser.h"
#include "forwarder.h"
#include "cache.h"

#define PROXY_PORT 8080
#define BACKLOG 10
#define CLIENT_BUFFER_SIZE MAX_HTTP_REQUEST
#define CLIENT_TIMEOUT_MS 10000

typedef struct
{
    SOCKET client_socket;
    struct sockaddr_in client_address;
} ClientContext;

static void send_error_response(
    SOCKET client_socket,
    int status_code,
    const char *status_text,
    const char *message
)
{
    char response[1024];

    int message_length = (int)strlen(message);

    int response_length = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code,
        status_text,
        message_length,
        message
    );

    if (response_length > 0)
    {
        send(
            client_socket,
            response,
            response_length,
            0
        );
    }
}

static void set_client_timeout(
    SOCKET client_socket
)
{
    int timeout = CLIENT_TIMEOUT_MS;

    setsockopt(
        client_socket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        (const char *)&timeout,
        sizeof(timeout)
    );

    setsockopt(
        client_socket,
        SOL_SOCKET,
        SO_SNDTIMEO,
        (const char *)&timeout,
        sizeof(timeout)
    );
}

static unsigned __stdcall handle_client(
    void *arg
)
{
    ClientContext *context;
    SOCKET client_socket;

    char request_buffer[CLIENT_BUFFER_SIZE];

    int bytes_received;
    int parse_result;
    int forward_result;
    int cache_result;

    HttpRequest parsed_request;

    context = (ClientContext *)arg;
    client_socket = context->client_socket;

    printf(
        "Client connected: %s:%d\n",
        inet_ntoa(
            context->client_address.sin_addr
        ),
        ntohs(
            context->client_address.sin_port
        )
    );

    free(context);

    set_client_timeout(client_socket);

    bytes_received = recv(
        client_socket,
        request_buffer,
        sizeof(request_buffer) - 1,
        0
    );

    if (bytes_received <= 0)
    {
        printf(
            "Failed to receive request.\n"
        );

        closesocket(client_socket);

        return 0;
    }

    request_buffer[bytes_received] = '\0';

    printf(
        "----- HTTP REQUEST -----\n%s"
        "------------------------\n",
        request_buffer
    );

    parse_result = parse_http_request(
        request_buffer,
        &parsed_request
    );

    if (parse_result == -1)
    {
        printf(
            "Malformed HTTP request.\n"
        );

        send_error_response(
            client_socket,
            400,
            "Bad Request",
            "Malformed HTTP request.\r\n"
        );

        closesocket(client_socket);

        return 0;
    }

    if (parse_result == -2)
    {
        printf(
            "Unsupported HTTP method.\n"
        );

        send_error_response(
            client_socket,
            501,
            "Not Implemented",
            "Only GET requests are supported.\r\n"
        );

        closesocket(client_socket);

        return 0;
    }

    printf(
        "===== PARSED REQUEST =====\n"
        "Method : %s\n"
        "Host   : %s\n"
        "Port   : %d\n"
        "Path   : %s\n"
        "==========================\n",
        parsed_request.method,
        parsed_request.host,
        parsed_request.port,
        parsed_request.path
    );
    if (is_domain_blocked(parsed_request.host))
{
    printf(
        "ACCESS DENIED: %s%s\n",
        parsed_request.host,
        parsed_request.path
    );

    send_error_response(
        client_socket,
        403,
        "Forbidden",
        "Access to this domain is blocked by the proxy.\r\n"
    );

    closesocket(client_socket);

    return 0;
}

    if (
        strcmp(
            parsed_request.method,
            "GET"
        ) == 0
    )
    {
        cache_result = cache_get(
            parsed_request.host,
            parsed_request.port,
            parsed_request.path,
            client_socket
        );

        if (cache_result == 1)
        {
            printf(
                "Response served from cache.\n"
            );

            closesocket(client_socket);

            return 0;
        }

        if (cache_result < 0)
        {
            printf(
                "Cache response failed.\n"
            );

            closesocket(client_socket);

            return 0;
        }
    }

    forward_result = forward_http_request(
        client_socket,
        request_buffer,
        &parsed_request
    );

    if (forward_result == 0)
    {
        printf(
            "HTTP response forwarded successfully.\n"
        );
    }
    else
    {
        printf(
            "HTTP forwarding failed. Error: %d\n",
            forward_result
        );
    }

    closesocket(client_socket);

    printf(
        "Client connection closed.\n"
    );

    return 0;
}

int main(void)
{
    WSADATA wsa_data;

    SOCKET server_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    int client_address_length;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa_data
        ) != 0
    )
    {
        printf(
            "WSAStartup failed.\n"
        );

        return 1;
    }

    printf(
        "Winsock initialized successfully.\n"
    );

    if (cache_init() != 0)
    {
        printf(
            "Cache initialization failed.\n"
        );

        WSACleanup();

        return 1;
    }

    printf(
        "Cache initialized successfully.\n"
    );

    server_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (
        server_socket == INVALID_SOCKET
    )
    {
        printf(
            "Socket creation failed.\n"
        );

        WSACleanup();

        return 1;
    }

    printf(
        "TCP socket created successfully.\n"
    );

    memset(
        &server_address,
        0,
        sizeof(server_address)
    );

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(
        PROXY_PORT
    );

    if (
        bind(
            server_socket,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == SOCKET_ERROR
    )
    {
        printf(
            "Bind failed.\n"
        );

        closesocket(server_socket);
        WSACleanup();

        return 1;
    }

    printf(
        "Proxy bound to port %d.\n",
        PROXY_PORT
    );

    if (
        listen(
            server_socket,
            BACKLOG
        ) == SOCKET_ERROR
    )
    {
        printf(
            "Listen failed.\n"
        );

        closesocket(server_socket);
        WSACleanup();

        return 1;
    }

    printf(
        "Proxy server is listening...\n"
    );

    while (1)
    {
        SOCKET client_socket;

        ClientContext *context;

        unsigned thread_handle;

        client_address_length =
            sizeof(client_address);

        client_socket = accept(
            server_socket,
            (struct sockaddr *)&client_address,
            &client_address_length
        );

        if (
            client_socket == INVALID_SOCKET
        )
        {
            printf(
                "Accept failed.\n"
            );

            continue;
        }

        context = (ClientContext *)malloc(
            sizeof(ClientContext)
        );

        if (context == NULL)
        {
            printf(
                "Memory allocation failed.\n"
            );

            closesocket(client_socket);

            continue;
        }

        context->client_socket =
            client_socket;

        context->client_address =
            client_address;

        thread_handle = _beginthreadex(
            NULL,
            0,
            handle_client,
            context,
            0,
            NULL
        );

        if (thread_handle == 0)
        {
            printf(
                "Thread creation failed.\n"
            );

            free(context);
            closesocket(client_socket);

            continue;
        }

        CloseHandle(
            (HANDLE)thread_handle
        );
    }

    closesocket(server_socket);

    WSACleanup();

    return 0;
}