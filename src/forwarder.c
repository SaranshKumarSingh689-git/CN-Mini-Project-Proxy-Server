#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "forwarder.h"
#include "cache.h"

#define FORWARD_BUFFER_SIZE 8192
#define MAX_FORWARD_REQUEST 16384
#define MAX_CACHED_RESPONSE (10 * 1024 * 1024)
#define CONNECTION_TIMEOUT_MS 10000

static int send_all(
    SOCKET socket,
    const char *data,
    int length
)
{
    int total_sent = 0;

    while (total_sent < length)
    {
        int sent = send(
            socket,
            data + total_sent,
            length - total_sent,
            0
        );

        if (sent == SOCKET_ERROR)
        {
            return -1;
        }

        total_sent += sent;
    }

    return 0;
}

static void set_socket_timeouts(
    SOCKET socket
)
{
    int timeout = CONNECTION_TIMEOUT_MS;

    setsockopt(
        socket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        (const char *)&timeout,
        sizeof(timeout)
    );

    setsockopt(
        socket,
        SOL_SOCKET,
        SO_SNDTIMEO,
        (const char *)&timeout,
        sizeof(timeout)
    );
}

static void send_proxy_error(
    SOCKET client_socket,
    int status_code,
    const char *status_text
)
{
    char response[1024];

    const char *body = "Proxy error.\r\n";

    int body_length = (int)strlen(body);

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
        body_length,
        body
    );

    if (response_length > 0)
    {
        send_all(
            client_socket,
            response,
            response_length
        );
    }
}

static int build_forward_request(
    const char *original_request,
    const HttpRequest *parsed_request,
    char *forward_request,
    int buffer_size
)
{
    const char *line_end;
    const char *headers_start;
    const char *headers_end;

    char headers[12000];
    int headers_length = 0;

    line_end = strstr(
        original_request,
        "\r\n"
    );

    if (line_end == NULL)
    {
        return -1;
    }

    headers_start = line_end + 2;

    headers_end = strstr(
        headers_start,
        "\r\n\r\n"
    );

    if (headers_end == NULL)
    {
        return -1;
    }

    {
        const char *current = headers_start;

        while (current < headers_end)
        {
            const char *next = strstr(
                current,
                "\r\n"
            );

            if (
                next == NULL ||
                next > headers_end
            )
            {
                break;
            }

            {
                int line_length =
                    (int)(next - current);

                if (
                    line_length > 0 &&
                    strncmp(
                        current,
                        "Proxy-Connection:",
                        18
                    ) != 0 &&
                    strncmp(
                        current,
                        "Connection:",
                        11
                    ) != 0 &&
                    strncmp(
                        current,
                        "Keep-Alive:",
                        11
                    ) != 0
                )
                {
                    if (
                        headers_length +
                        line_length +
                        2 >=
                        (int)sizeof(headers)
                    )
                    {
                        return -1;
                    }

                    memcpy(
                        headers + headers_length,
                        current,
                        line_length
                    );

                    headers_length += line_length;

                    headers[headers_length++] = '\r';
                    headers[headers_length++] = '\n';
                }
            }

            current = next + 2;
        }
    }

    {
        int required_size;

        required_size = snprintf(
            forward_request,
            buffer_size,
            "%s %s HTTP/1.1\r\n",
            parsed_request->method,
            parsed_request->path
        );

        if (
            required_size < 0 ||
            required_size >= buffer_size
        )
        {
            return -1;
        }

        if (
            required_size +
            headers_length +
            22 >= buffer_size
        )
        {
            return -1;
        }

        memcpy(
            forward_request + required_size,
            headers,
            headers_length
        );

        required_size += headers_length;

        required_size += snprintf(
            forward_request + required_size,
            buffer_size - required_size,
            "Connection: close\r\n"
            "\r\n"
        );

        return required_size;
    }
}

int forward_http_request(
    SOCKET client_socket,
    const char *original_request,
    const HttpRequest *parsed_request
)
{
    struct hostent *host_entry;
    struct sockaddr_in server_address;

    SOCKET server_socket;

    char forward_request[MAX_FORWARD_REQUEST];
    char buffer[FORWARD_BUFFER_SIZE];

    char *cached_response = NULL;

    int request_length;
    int bytes_received;

    int cached_length = 0;

    host_entry = gethostbyname(
        parsed_request->host
    );

    if (host_entry == NULL)
    {
        send_proxy_error(
            client_socket,
            502,
            "Bad Gateway"
        );

        return -2;
    }

    server_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (server_socket == INVALID_SOCKET)
    {
        send_proxy_error(
            client_socket,
            502,
            "Bad Gateway"
        );

        return -3;
    }

    set_socket_timeouts(
        server_socket
    );

    memset(
        &server_address,
        0,
        sizeof(server_address)
    );

    server_address.sin_family = AF_INET;

    server_address.sin_port = htons(
        (u_short)parsed_request->port
    );

    memcpy(
        &server_address.sin_addr,
        host_entry->h_addr_list[0],
        host_entry->h_length
    );

    if (
        connect(
            server_socket,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == SOCKET_ERROR
    )
    {
        int error_code =
            WSAGetLastError();

        closesocket(server_socket);

        if (
            error_code == WSAETIMEDOUT ||
            error_code == WSAEWOULDBLOCK
        )
        {
            send_proxy_error(
                client_socket,
                504,
                "Gateway Timeout"
            );

            return -4;
        }

        send_proxy_error(
            client_socket,
            502,
            "Bad Gateway"
        );

        return -5;
    }

    request_length = build_forward_request(
        original_request,
        parsed_request,
        forward_request,
        sizeof(forward_request)
    );

    if (request_length < 0)
    {
        closesocket(server_socket);

        send_proxy_error(
            client_socket,
            400,
            "Bad Request"
        );

        return -6;
    }

    if (
        send_all(
            server_socket,
            forward_request,
            request_length
        ) < 0
    )
    {
        closesocket(server_socket);

        send_proxy_error(
            client_socket,
            502,
            "Bad Gateway"
        );

        return -7;
    }

    cached_response =
        (char *)malloc(
            MAX_CACHED_RESPONSE
        );

    while (1)
    {
        bytes_received = recv(
            server_socket,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytes_received == 0)
        {
            break;
        }

        if (bytes_received == SOCKET_ERROR)
        {
            int error_code =
                WSAGetLastError();

            closesocket(server_socket);

            free(cached_response);

            if (
                error_code == WSAETIMEDOUT ||
                error_code == WSAEWOULDBLOCK
            )
            {
                send_proxy_error(
                    client_socket,
                    504,
                    "Gateway Timeout"
                );

                return -8;
            }

            return -9;
        }

        if (
            send_all(
                client_socket,
                buffer,
                bytes_received
            ) < 0
        )
        {
            closesocket(server_socket);

            free(cached_response);

            return -10;
        }

        if (
            cached_response != NULL &&
            cached_length +
            bytes_received <=
            MAX_CACHED_RESPONSE
        )
        {
            memcpy(
                cached_response + cached_length,
                buffer,
                bytes_received
            );

            cached_length += bytes_received;
        }
        else
        {
            free(cached_response);
            cached_response = NULL;
        }
    }

    closesocket(server_socket);

    if (
        cached_response != NULL &&
        cached_length > 0
    )
    {
        if (
            cached_response[0] == 'H' &&
            cached_response[1] == 'T' &&
            cached_response[2] == 'T' &&
            cached_response[3] == 'P'
        )
        {
            cache_store(
                parsed_request->host,
                parsed_request->port,
                parsed_request->path,
                cached_response,
                cached_length
            );
        }

        free(cached_response);
    }

    return 0;
}