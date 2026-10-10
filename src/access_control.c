#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "access_control.h"

#define MAX_DOMAIN_LENGTH 256
#define MAX_LINE_LENGTH 512

static void trim_whitespace(char *text)
{
    char *start = text;
    char *end;

    while (*start && isspace((unsigned char)*start))
    {
        start++;
    }

    if (start != text)
    {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);

    while (end > text && isspace((unsigned char)end[-1]))
    {
        end--;
    }

    *end = '\0';
}

static void lowercase_string(char *text)
{
    while (*text)
    {
        *text = (char)tolower((unsigned char)*text);
        text++;
    }
}

static int domain_matches(
    const char *host,
    const char *blocked_domain
)
{
    size_t host_length = strlen(host);
    size_t blocked_length = strlen(blocked_domain);

    if (host_length < blocked_length)
    {
        return 0;
    }

    if (
        strcmp(
            host + host_length - blocked_length,
            blocked_domain
        ) != 0
    )
    {
        return 0;
    }

    if (host_length == blocked_length)
    {
        return 1;
    }

    return host[host_length - blocked_length - 1] == '.';
}

int is_domain_blocked(const char *host)
{
    FILE *file;
    char normalized_host[MAX_DOMAIN_LENGTH];
    char line[MAX_LINE_LENGTH];

    if (host == NULL || host[0] == '\0')
    {
        return 0;
    }

    if (strlen(host) >= sizeof(normalized_host))
    {
        return 0;
    }

    strcpy(normalized_host, host);
    lowercase_string(normalized_host);

    file = fopen(BLOCKED_DOMAINS_FILE, "r");

    if (file == NULL)
    {
        printf(
            "WARNING: Could not open %s. "
            "Access control is unavailable.\n",
            BLOCKED_DOMAINS_FILE
        );

        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        trim_whitespace(line);

        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        lowercase_string(line);

        if (domain_matches(normalized_host, line))
        {
            fclose(file);
            return 1;
        }
    }

    fclose(file);

    return 0;
}