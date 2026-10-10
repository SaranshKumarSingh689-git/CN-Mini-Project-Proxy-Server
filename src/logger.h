#ifndef LOGGER_H
#define LOGGER_H

int logger_init(void);

void log_request(
const char *client_ip,
const char *method,
const char *host,
const char *path,
const char *access_decision,
const char *cache_status,
const char *outcome,
long response_time_ms
);

#endif
