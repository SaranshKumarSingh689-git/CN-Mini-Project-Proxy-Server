#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "http_parser.h"

static int parse_host(
    const char *host_value,
    HttpRequest *parsed_request
)
{
    char host_copy[MAX_HOST_LENGTH];
    char *colon;
    char *end;
    long port;

    if (host_value == NULL || parsed_request == NULL)
    {
        return -1;
    }

    strncpy(
        host_copy,
        host_value,
        sizeof(host_copy) - 1
    );

    host_copy[sizeof(host_copy) - 1] = '\0';

    end = host_copy + strlen(host_copy);

    while (
        end > host_copy &&
        (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ')
    )
    {
        end--;
        *end = '\0';
    }

    while (*host_copy == ' ')
    {
        memmove(
            host_copy,
            host_copy + 1,
            strlen(host_copy)
        );
    }

    if (host_copy[0] == '\0')
    {
        return -1;
    }

    colon = strrchr(host_copy, ':');

    if (colon != NULL)
    {
        *colon = '\0';

        if (*(colon + 1) == '\0')
        {
            return -1;
        }

        port = strtol(
            colon + 1,
            &end,
            10
        );

        if (*end != '\0' || port < 1 || port > 65535)
        {
            return -1;
        }

        parsed_request->port = (int)port;
    }
    else
    {
        parsed_request->port = 80;
    }

    if (host_copy[0] == '\0')
    {
        return -1;
    }

    strncpy(
        parsed_request->host,
        host_copy,
        MAX_HOST_LENGTH - 1
    );

    parsed_request->host[MAX_HOST_LENGTH - 1] = '\0';

    return 0;
}

int parse_http_request(
    const char *request,
    HttpRequest *parsed_request
)
{
    char method[MAX_METHOD_LENGTH];
    char url[MAX_PATH_LENGTH];
    char version[16];

    const char *line_end;
    const char *host_header;

    if (
        request == NULL ||
        parsed_request == NULL
    )
    {
        return -1;
    }

    memset(
        parsed_request,
        0,
        sizeof(HttpRequest)
    );

    line_end = strstr(
        request,
        "\r\n"
    );

    if (line_end == NULL)
    {
        return -1;
    }

    if (
        sscanf(
            request,
            "%15s %2047s %15s",
            method,
            url,
            version
        ) != 3
    )
    {
        return -1;
    }

    if (
        strcmp(method, "GET") != 0
    )
    {
        return -2;
    }

    if (
        strcmp(version, "HTTP/1.0") != 0 &&
        strcmp(version, "HTTP/1.1") != 0
    )
    {
        return -1;
    }

    strncpy(
        parsed_request->method,
        method,
        MAX_METHOD_LENGTH - 1
    );

    parsed_request->method[
        MAX_METHOD_LENGTH - 1
    ] = '\0';

    if (
        strncmp(
            url,
            "http://",
            7
        ) == 0
    )
    {
        char url_copy[MAX_PATH_LENGTH];
        char *host_start;
        char *path_start;

        strncpy(
            url_copy,
            url + 7,
            sizeof(url_copy) - 1
        );

        url_copy[
            sizeof(url_copy) - 1
        ] = '\0';

        host_start = url_copy;

        path_start = strchr(
            host_start,
            '/'
        );

        if (path_start != NULL)
        {
            strncpy(
                parsed_request->path,
                path_start,
                MAX_PATH_LENGTH - 1
            );

            parsed_request->path[
                MAX_PATH_LENGTH - 1
            ] = '\0';

            *path_start = '\0';
        }
        else
        {
            strcpy(
                parsed_request->path,
                "/"
            );
        }

        if (
            parse_host(
                host_start,
                parsed_request
            ) != 0
        )
        {
            return -1;
        }
    }
    else
    {
        if (url[0] != '/')
        {
            return -1;
        }

        strncpy(
            parsed_request->path,
            url,
            MAX_PATH_LENGTH - 1
        );

        parsed_request->path[
            MAX_PATH_LENGTH - 1
        ] = '\0';

        host_header = strstr(
            request,
            "\r\nHost:"
        );

        if (host_header == NULL)
        {
            host_header = strstr(
                request,
                "\r\nhost:"
            );
        }

        if (host_header == NULL)
        {
            return -1;
        }

        host_header += 7;

        while (*host_header == ' ')
        {
            host_header++;
        }

        {
            char host_value[MAX_HOST_LENGTH];
            const char *header_end;
            int length;

            header_end = strstr(
                host_header,
                "\r\n"
            );

            if (header_end == NULL)
            {
                return -1;
            }

            length = (int)(
                header_end - host_header
            );

            if (
                length <= 0 ||
                length >= MAX_HOST_LENGTH
            )
            {
                return -1;
            }

            memcpy(
                host_value,
                host_header,
                length
            );

            host_value[length] = '\0';

            if (
                parse_host(
                    host_value,
                    parsed_request
                ) != 0
            )
            {
                return -1;
            }
        }
    }

    if (
        parsed_request->host[0] == '\0'
    )
    {
        return -1;
    }

    if (
        parsed_request->path[0] == '\0'
    )
    {
        strcpy(
            parsed_request->path,
            "/"
        );
    }

    return 0;
}