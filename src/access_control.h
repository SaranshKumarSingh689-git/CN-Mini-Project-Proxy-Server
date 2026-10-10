#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#define BLOCKED_DOMAINS_FILE "config/blocked_domains.txt"

int is_domain_blocked(const char *host);

#endif