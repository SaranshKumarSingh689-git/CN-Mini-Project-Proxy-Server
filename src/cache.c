#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <sys/stat.h>

#include "cache.h"

#define CACHE_DIRECTORY "cache"
#define CACHE_KEY_LENGTH 4096
#define CACHE_PATH_LENGTH 512

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

static void build_cache_key(
    const char *host,
    int port,
    const char *path,
    char *key,
    int key_size
)
{
    snprintf(
        key,
        key_size,
        "%s_%d_%s",
        host,
        port,
        path
    );
}

static unsigned long hash_string(
    const char *string
)
{
    unsigned long hash = 5381;
    int character;

    while ((character = *string++))
    {
        hash = (
            (hash << 5) +
            hash
        ) + character;
    }

    return hash;
}

static void build_cache_path(
    const char *host,
    int port,
    const char *path,
    char *cache_path,
    int cache_path_size
)
{
    char key[CACHE_KEY_LENGTH];
    unsigned long hash;

    build_cache_key(
        host,
        port,
        path,
        key,
        sizeof(key)
    );

    hash = hash_string(key);

    snprintf(
        cache_path,
        cache_path_size,
        "%s/%lu.cache",
        CACHE_DIRECTORY,
        hash
    );
}

int cache_init(void)
{
    struct _stat directory_info;

    if (
        _stat(
            CACHE_DIRECTORY,
            &directory_info
        ) == 0
    )
    {
        if (
            (directory_info.st_mode & _S_IFDIR) != 0
        )
        {
            return 0;
        }

        return -1;
    }

    if (
        _mkdir(CACHE_DIRECTORY) != 0
    )
    {
        return -1;
    }

    return 0;
}

int cache_get(
    const char *host,
    int port,
    const char *path,
    SOCKET client_socket
)
{
    char cache_path[CACHE_PATH_LENGTH];

    FILE *file;

    long file_size;
    time_t current_time;
    time_t modified_time;

    char *response;

    struct _stat file_info;

    build_cache_path(
        host,
        port,
        path,
        cache_path,
        sizeof(cache_path)
    );

    file = fopen(
        cache_path,
        "rb"
    );

    if (file == NULL)
    {
        printf(
            "CACHE MISS: %s%s\n",
            host,
            path
        );

        return 0;
    }

    if (
        _stat(
            cache_path,
            &file_info
        ) != 0
    )
    {
        fclose(file);
        return 0;
    }

    modified_time = file_info.st_mtime;
    current_time = time(NULL);

    if (
        current_time - modified_time >
        CACHE_TTL_SECONDS
    )
    {
        fclose(file);

        remove(cache_path);

        printf(
            "CACHE EXPIRED: %s%s\n",
            host,
            path
        );

        return 0;
    }

    if (
        fseek(
            file,
            0,
            SEEK_END
        ) != 0
    )
    {
        fclose(file);
        return 0;
    }

    file_size = ftell(file);

    if (file_size <= 0)
    {
        fclose(file);
        return 0;
    }

    if (
        fseek(
            file,
            0,
            SEEK_SET
        ) != 0
    )
    {
        fclose(file);
        return 0;
    }

    response = (char *)malloc(
        file_size
    );

    if (response == NULL)
    {
        fclose(file);
        return 0;
    }

    if (
        fread(
            response,
            1,
            file_size,
            file
        ) != (size_t)file_size
    )
    {
        free(response);
        fclose(file);

        return 0;
    }

    fclose(file);

    if (
        send_all(
            client_socket,
            response,
            (int)file_size
        ) != 0
    )
    {
        free(response);
        return -1;
    }

    free(response);

    printf(
        "CACHE HIT: %s%s\n",
        host,
        path
    );

    return 1;
}

int cache_store(
    const char *host,
    int port,
    const char *path,
    const char *response,
    int response_length
)
{
    char cache_path[CACHE_PATH_LENGTH];

    FILE *file;

    build_cache_path(
        host,
        port,
        path,
        cache_path,
        sizeof(cache_path)
    );

    file = fopen(
        cache_path,
        "wb"
    );

    if (file == NULL)
    {
        return -1;
    }

    if (
        fwrite(
            response,
            1,
            response_length,
            file
        ) != (size_t)response_length
    )
    {
        fclose(file);
        remove(cache_path);

        return -1;
    }

    fclose(file);

    printf(
        "CACHE STORED: %s%s\n",
        host,
        path
    );

    return 0;
}