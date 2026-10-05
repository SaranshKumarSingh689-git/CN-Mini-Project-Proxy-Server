#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "http_parser.h"

static int parse_host(
    const char *host_value,
    char *host,
    int *port
)
{
    char host_copy[MAX_HOST_LENGTH];

    strncpy(
        host_copy,
        host_value,
        sizeof(host_copy) - 1
    );

    host_copy[sizeof(host_copy) - 1] = '\0';

    /*
     * Remove trailing whitespace/newline.
     */
    char *newline = strpbrk(host_copy, "\r\n");

    if (newline != NULL)
    {
        *newline = '\0';
    }

    /*
     * Check whether the Host header contains a port.
     *
     * Example:
     *
     * example.com
     * example.com:8080
     */

    char *colon = strchr(host_copy, ':');

    if (colon != NULL)
    {
        *colon = '\0';

        colon++;

        int parsed_port = atoi(colon);

        if (parsed_port <= 0 || parsed_port > 65535)
        {
            return -1;
        }

        *port = parsed_port;
    }
    else
    {
        *port = 80;
    }

    if (strlen(host_copy) == 0)
    {
        return -1;
    }

    strncpy(
        host,
        host_copy,
        MAX_HOST_LENGTH - 1
    );

    host[MAX_HOST_LENGTH - 1] = '\0';

    return 0;
}


int parse_http_request(
    const char *request,
    HttpRequest *parsed_request
)
{
    if (request == NULL || parsed_request == NULL)
    {
        return -1;
    }

    memset(
        parsed_request,
        0,
        sizeof(HttpRequest)
    );

    /*
     * Find the first line.
     *
     * Example:
     *
     * GET http://example.com/ HTTP/1.1
     */

    char first_line[4096];

    const char *line_end = strstr(
        request,
        "\r\n"
    );

    if (line_end == NULL)
    {
        return -1;
    }

    size_t line_length =
        (size_t)(line_end - request);

    if (line_length >= sizeof(first_line))
    {
        return -1;
    }

    memcpy(
        first_line,
        request,
        line_length
    );

    first_line[line_length] = '\0';

    /*
     * Parse:
     *
     * METHOD URL HTTP_VERSION
     */

    char method[MAX_METHOD_LENGTH];
    char url[2048];
    char http_version[32];

    int fields = sscanf(
        first_line,
        "%15s %2047s %31s",
        method,
        url,
        http_version
    );

    if (fields != 3)
    {
        return -1;
    }

    /*
     * Currently we support GET.
     *
     * Other methods can be added later if required.
     */

    if (strcmp(method, "GET") != 0)
    {
        return -2;
    }

    strncpy(
        parsed_request->method,
        method,
        MAX_METHOD_LENGTH - 1
    );

    parsed_request->method[
        MAX_METHOD_LENGTH - 1
    ] = '\0';


    /*
     * Extract path from URL.
     *
     * Proxy requests normally contain:
     *
     * http://example.com/path
     */

    char host_from_url[MAX_HOST_LENGTH];

    memset(
        host_from_url,
        0,
        sizeof(host_from_url)
    );

    if (strncmp(url, "http://", 7) == 0)
    {
        char *url_start = url + 7;

        char *path_start = strchr(
            url_start,
            '/'
        );

        if (path_start != NULL)
        {
            size_t host_length =
                (size_t)(path_start - url_start);

            if (host_length >= sizeof(host_from_url))
            {
                return -1;
            }

            memcpy(
                host_from_url,
                url_start,
                host_length
            );

            host_from_url[host_length] = '\0';

            strncpy(
                parsed_request->path,
                path_start,
                MAX_PATH_LENGTH - 1
            );
        }
        else
        {
            strncpy(
                host_from_url,
                url_start,
                sizeof(host_from_url) - 1
            );

            strcpy(
                parsed_request->path,
                "/"
            );
        }

        /*
         * Parse host and optional port.
         */

        if (parse_host(
                host_from_url,
                parsed_request->host,
                &parsed_request->port
            ) != 0)
        {
            return -1;
        }
    }
    else
    {
        /*
         * Handle origin-form requests.
         *
         * Example:
         *
         * GET /index.html HTTP/1.1
         *
         * The actual host is obtained from the Host header.
         */

        strncpy(
            parsed_request->path,
            url,
            MAX_PATH_LENGTH - 1
        );

        const char *host_header =
            strstr(request, "\r\nHost:");

        if (host_header == NULL)
        {
            host_header =
                strstr(request, "\nhost:");
        }

        if (host_header == NULL)
        {
            return -1;
        }

        const char *host_value =
            strchr(host_header, ':');

        if (host_value == NULL)
        {
            return -1;
        }

        host_value++;

        while (*host_value == ' ')
        {
            host_value++;
        }

        if (parse_host(
                host_value,
                parsed_request->host,
                &parsed_request->port
            ) != 0)
        {
            return -1;
        }
    }

    return 0;
}