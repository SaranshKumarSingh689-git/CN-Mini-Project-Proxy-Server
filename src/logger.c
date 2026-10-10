#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "logger.h"

#define LOG_DIRECTORY "logs"
#define LOG_FILE "logs/proxy.log"

static CRITICAL_SECTION log_lock;
static int logger_initialized = 0;

int logger_init(void)
{
DWORD attributes;
if (logger_initialized)
{
    return 0;
}

attributes = GetFileAttributesA(LOG_DIRECTORY);

if (attributes == INVALID_FILE_ATTRIBUTES)
{
    if (!CreateDirectoryA(LOG_DIRECTORY, NULL))
    {
        printf("Failed to create log directory.\n");
        return -1;
    }
}
else if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
{
    printf("The logs path is not a directory.\n");
    return -1;
}

InitializeCriticalSection(&log_lock);
logger_initialized = 1;

return 0;

}

void log_request(
const char *client_ip,
const char *method,
const char *host,
const char *path,
const char *access_decision,
const char *cache_status,
const char *outcome,
long response_time_ms
)
{
FILE *file;

time_t current_time;
struct tm *time_info;
struct tm time_copy;

char timestamp[32];

if (!logger_initialized)
{
    return;
}

EnterCriticalSection(&log_lock);

current_time = time(NULL);
time_info = localtime(&current_time);

if (time_info == NULL)
{
    LeaveCriticalSection(&log_lock);
    return;
}

time_copy = *time_info;

strftime(
    timestamp,
    sizeof(timestamp),
    "%Y-%m-%d %H:%M:%S",
    &time_copy
);

file = fopen(LOG_FILE, "a");

if (file != NULL)
{
    fprintf(
        file,
        "%s | %s | %s | %s | %s | %s | %s | %s | %ld ms\n",
        timestamp,
        client_ip != NULL ? client_ip : "-",
        method != NULL ? method : "-",
        host != NULL ? host : "-",
        path != NULL ? path : "-",
        access_decision != NULL ? access_decision : "-",
        cache_status != NULL ? cache_status : "-",
        outcome != NULL ? outcome : "-",
        response_time_ms
    );

    fclose(file);
}
else
{
    printf(
        "Failed to write to %s.\n",
        LOG_FILE
    );
}

LeaveCriticalSection(&log_lock);

}
