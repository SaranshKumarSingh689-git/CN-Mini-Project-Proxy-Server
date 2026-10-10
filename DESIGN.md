# Design Document: HTTP Proxy Server

## 1. Introduction

This document describes the design of an HTTP proxy server implemented in C using the Winsock networking API on Windows.

The proxy acts as an intermediary between HTTP clients and destination servers. It receives client requests, validates them, applies domain-based access control, checks the response cache, forwards eligible cache misses to the destination, and returns responses to clients.

The implementation also supports concurrent client handling and traffic logging.

## 2. Objectives

The primary objectives are:

* Implement a TCP server using Winsock.
* Parse and validate HTTP requests.
* Forward HTTP GET requests to destination servers.
* Handle multiple clients concurrently.
* Cache eligible responses on disk.
* Block configured domains and their subdomains.
* Handle malformed requests and upstream failures.
* Record request outcomes and processing times.
* Compare direct, uncached proxy, and cached response times.

## 3. System Architecture

The implementation is organized into the following components.

### 3.1 Main Server (`main.c`)

Responsible for:

* Initializing Winsock and the listening socket.
* Accepting incoming client connections.
* Dispatching clients to worker threads.
* Coordinating request parsing, access control, caching, forwarding, and logging.
* Sending appropriate HTTP error responses.

### 3.2 HTTP Parser (`http_parser.c`, `http_parser.h`)

Responsible for:

* Parsing incoming HTTP request data.
* Extracting the method, destination host, port, and request path.
* Validating supported request formats.
* Identifying malformed requests and unsupported methods.

The proxy currently supports HTTP GET requests.

### 3.3 Forwarder (`forwarder.c`, `forwarder.h`)

Responsible for:

* Resolving destination hostnames.
* Establishing connections to destination servers.
* Forwarding supported HTTP requests.
* Receiving destination responses.
* Returning responses or reporting forwarding failures.

### 3.4 Cache (`cache.c`, `cache.h`)

Responsible for:

* Creating cache keys from request destination information.
* Storing eligible responses on disk.
* Retrieving valid cached responses.
* Tracking cache hits, misses, and expiration.

The cache uses a time-to-live (TTL) of 60 seconds.

### 3.5 Access Control (`access_control.c`, `access_control.h`)

Responsible for:

* Reading configured blocked domains from `config/blocked_domains.txt`.
* Comparing destination domains against the blocked list.
* Blocking matching domains and their subdomains.
* Returning an access decision to the main request handler.

Access control is evaluated before cache lookup so that a cached response cannot bypass a blocking rule.

### 3.6 Logger (`logger.c`, `logger.h`)

Responsible for appending request records to `logs/proxy.log`.

Each record includes:

* Timestamp
* Client IP address
* HTTP method
* Destination host and path
* Access-control decision
* Cache status
* Request outcome
* Processing time

Synchronization is used to protect concurrent log writes.

## 4. Request Processing Flow

A typical request follows this sequence:

1. The client connects to the proxy's listening TCP socket.
2. The server accepts the connection.
3. A worker thread handles the client.
4. The request is parsed and validated.
5. Malformed requests are rejected with HTTP 400.
6. Unsupported HTTP methods are rejected with HTTP 501.
7. The destination domain is checked against the blocked-domain configuration.
8. Blocked destinations receive HTTP 403.
9. For an allowed request, the proxy checks the cache.
10. If a valid cache entry exists, the cached response is returned.
11. Otherwise, the request is forwarded to the destination server.
12. The destination response is returned to the client.
13. Eligible responses are stored in the cache.
14. The request outcome and processing time are recorded in the traffic log.
15. The client connection is closed.

If DNS resolution or upstream communication fails, the proxy returns an appropriate gateway error when possible.

## 5. Caching Design

### 5.1 Purpose

Caching avoids repeatedly fetching the same eligible response from a destination server. It can reduce network traffic and response time for repeated requests.

### 5.2 Cache Key

The cache key incorporates destination information such as:

* Host
* Port
* Request path

This allows different destinations or paths to have separate cache entries.

### 5.3 Cache Miss

When a valid cache entry is unavailable, the proxy forwards the request to the destination server. An eligible response may then be stored in the cache.

### 5.4 Cache Hit

When a valid entry exists, the proxy serves the cached response without contacting the destination server.

### 5.5 Expiration

Cache entries use a 60-second TTL. Expired entries are not intended to be served as valid cache hits; the proxy fetches the response again when required.

### 5.6 Access-Control Interaction

The access-control check occurs before cache lookup. Consequently, a blocked domain is rejected even if a cached response exists for that domain.

## 6. Domain-Based Access Control

The file `config/blocked_domains.txt` contains the configured blocked domains.

Example:

text
# Blocked domains
example.com


Blank lines and lines beginning with `#` are treated as configuration comments or ignored entries.

Domain comparisons are case-insensitive. The configured domain and its subdomains are blocked.

For example, blocking `example.com` should also block `www.example.com`.

A blocked request receives HTTP 403 Forbidden.

## 7. Concurrency

The server uses Windows-compatible multithreading to handle multiple client connections.

Each accepted client is assigned to a worker thread, allowing the server to process other clients while a worker handles its request.

The logger uses synchronization to protect concurrent writes. Concurrent cache requests can potentially perform duplicate upstream fetches if multiple workers observe a cache miss before a response has been stored.

## 8. Error Handling

The implementation has been tested with the following error conditions:

| Condition                   | Observed HTTP response |
| --------------------------- | ---------------------- |
| Malformed request           | 400 Bad Request        |
| Unsupported method          | 501 Not Implemented    |
| DNS resolution failure      | 502 Bad Gateway        |
| Upstream connection timeout | 504 Gateway Timeout    |
| Blocked destination         | 403 Forbidden          |

The proxy logs forwarding failures and returns an error response when the failure-handling path can complete.

## 9. Traffic Logging

Traffic records are appended to `logs/proxy.log`.

Example record:

text
2026-10-10 20:30:02 | 127.0.0.1 | GET | example.com | / | ALLOWED | MISS | FORWARDED | 110 ms


The record describes the timestamp, client, request, access decision, cache status, outcome, and processing time.

These logs are useful for debugging, verifying cache behavior, observing access-control decisions, and evaluating request handling.

## 10. Testing Strategy

The implementation has undergone the following functional tests:

* Successful HTTP forwarding
* Cache miss and cache hit
* Cache expiration and refetch
* Blocking a configured domain
* Blocking a subdomain
* Blocking a domain with an existing cache entry
* Malformed request handling
* Unsupported method handling
* Invalid DNS hostname handling
* Unreachable destination handling
* Multiple concurrent client requests
* Traffic-log verification

Performance tests compared a direct HTTP request, an uncached proxy request, and cached proxy requests. The measurements are preliminary and depend on network conditions.

## 11. Limitations

* Only HTTP GET requests are supported.
* HTTPS CONNECT tunnelling is not implemented.
* POST and other unsupported methods are rejected.
* The cache TTL is fixed at 60 seconds.
* Concurrent requests can result in duplicate upstream fetches during simultaneous cache misses.
* Upstream connection failures can take several seconds to time out.
* Performance results are affected by destination-server behavior and network conditions.

## 12. Conclusion

The project demonstrates a functional HTTP proxy server using C and Winsock. It combines TCP socket programming, HTTP parsing and forwarding, concurrent client handling, disk-based caching, configurable domain filtering, error handling, and traffic logging.

The implementation and tests provide practical experience with several core computer networking concepts and illustrate how intermediary servers can control, observe, and optimize HTTP request handling.
