# Computer Networks Mini Project

## Designing and Implementing an HTTP Proxy Server

### Student Details

* **Name:** Saransh Kumar Singh
* **Roll Number:** 24158020
* **Group:** Individual
* **Language:** C
* **Platform:** Windows
* **Networking API:** Winsock
* **Protocol:** HTTP

## 1. Project Overview

This project implements an HTTP proxy server in C using the Winsock networking API on Windows.

A proxy server acts as an intermediary between a client and a destination server. Instead of connecting directly to the destination, a client sends its HTTP request through the proxy, which processes the request and forwards it when permitted.

The project demonstrates TCP socket programming, HTTP request parsing, concurrent client handling, response caching, domain-based access control, error handling, and traffic logging.

## 2. Implemented Features

### HTTP Request Parsing

* Accepts and parses HTTP requests.
* Supports HTTP GET requests.
* Validates incoming requests and handles malformed input.
* Supports absolute-form URLs used by HTTP clients communicating through a proxy.

### HTTP Request Forwarding

* Connects to destination servers.
* Forwards supported HTTP requests.
* Returns destination responses to clients.
* Handles DNS resolution and connection failures.

### Concurrent Client Handling

* Handles multiple clients using Windows threads.
* Allows multiple requests to be processed without requiring a separate server instance for each client.

### Response Caching

* Stores eligible HTTP responses on disk.
* Serves cached responses for subsequent matching requests.
* Records cache misses, hits, and expiration events.
* Uses a cache time-to-live (TTL) of 60 seconds.

### Domain-Based Access Control

* Reads blocked domains from `config/blocked_domains.txt`.
* Supports case-insensitive domain matching.
* Blocks the specified domain and its subdomains.
* Returns HTTP 403 for blocked requests.
* Checks access permissions before attempting to serve a cached response.

### Error Handling

The proxy handles several error conditions, including:

* Malformed HTTP requests: HTTP 400.
* Unsupported HTTP methods: HTTP 501.
* DNS resolution failures: HTTP 502.
* Upstream connection timeouts: HTTP 504.

### Traffic Logging

The proxy records request information in `logs/proxy.log`, including:

* Timestamp
* Client IP address
* HTTP method
* Destination host and path
* Access-control decision
* Cache status
* Request outcome
* Processing time

## 3. Technology Stack

| Component            | Technology                 |
| -------------------- | -------------------------- |
| Programming language | C                          |
| Networking           | Winsock / TCP sockets      |
| Concurrency          | Windows threads            |
| Application protocol | HTTP                       |
| Caching              | Disk-based response cache  |
| Access control       | Configurable domain list   |
| Logging              | File-based traffic logging |
| Compiler             | GCC / MinGW                |
| Version control      | Git and GitHub             |

## 4. Project Structure

text
CN_MINI_PROJECT/
├── config/
│   └── blocked_domains.txt
├── src/
│   ├── main.c
│   ├── http_parser.c
│   ├── http_parser.h
│   ├── forwarder.c
│   ├── forwarder.h
│   ├── cache.c
│   ├── cache.h
│   ├── access_control.c
│   ├── access_control.h
│   ├── logger.c
│   └── logger.h
├── cache/
├── logs/
├── tests/
├── DESIGN.md
├── README.md
└── .gitignore


The `cache/` directory stores cached response data, while `logs/` stores proxy traffic logs. Runtime files and generated binaries should not be committed unless required by the assignment.

## 5. Compilation and Execution

### Prerequisites

* Windows operating system
* GCC compiler with MinGW
* Winsock libraries available through the Windows toolchain
* `curl.exe` for testing

### Compile

Open PowerShell in the project directory and run:

powershell
gcc src/main.c src/http_parser.c src/forwarder.c src/cache.c src/access_control.c src/logger.c -o proxy.exe -lws2_32


### Start the proxy

powershell
.\proxy.exe


The examples below assume that the proxy listens on `127.0.0.1:8080`.

### Test HTTP forwarding

powershell
curl.exe -i -x http://127.0.0.1:8080 http://example.com/

### Test response caching

Run the same request twice:

powershell
curl.exe -i -x http://127.0.0.1:8080 http://example.com/


The first request should produce a cache miss when no valid cached response exists. A subsequent request within the 60-second TTL should produce a cache hit.

### Test domain blocking

Edit `config/blocked_domains.txt` and add:

text
example.com


Then run:

powershell
curl.exe -i -x http://127.0.0.1:8080 http://example.com/


The expected result is HTTP 403.

Remove or comment out the domain entry after testing if you want to allow the domain again.

### Inspect traffic logs

powershell
Get-Content .\logs\proxy.log -Tail 10


## 6. Testing and Validation

The following tests were performed during development:

| Test                    | Observed result                                |
| ----------------------- | ---------------------------------------------- |
| Normal HTTP forwarding  | HTTP 200                                       |
| Cache miss              | Request forwarded to destination               |
| Cache hit               | Response served from cache                     |
| Cache expiration        | Expired response fetched again                 |
| Blocked domain          | HTTP 403                                       |
| Blocked subdomain       | HTTP 403                                       |
| Malformed request       | HTTP 400                                       |
| Unsupported POST method | HTTP 501                                       |
| Invalid DNS hostname    | HTTP 502                                       |
| Unreachable destination | HTTP 504                                       |
| Concurrent requests     | Five requests processed successfully           |
| Traffic logging         | Request outcomes and processing times recorded |

The tests demonstrate the basic operation of the proxy and its main error-handling paths. They do not constitute exhaustive security, load, or protocol-compliance testing.

## 7. Preliminary Performance Measurements

The following response times were measured during development:

| Request type           | Observed response time |
| ---------------------- | ---------------------: |
| Direct HTTP request    |             135.287 ms |
| Uncached proxy request |             116.401 ms |
| Cached proxy request   |              24.104 ms |

Additional cached-request measurements ranged from approximately 2.6 ms to 14.2 ms.

These are preliminary measurements rather than controlled benchmark results. Response times vary with network conditions, destination-server behavior, and system load. Further repeated trials are needed for a reliable performance comparison.

## 8. Limitations

* The implementation currently supports HTTP GET requests only.
* HTTPS tunnelling using the CONNECT method is not implemented.
* POST and other unsupported methods are rejected.
* The 60-second cache TTL is fixed in the implementation.
* Performance measurements depend on external network conditions.
* The project has been tested in the Windows environment for which it was developed; portability to other operating systems has not been established.

## 9. Learning Outcomes

This project provided practical experience with:

* TCP socket programming using Winsock
* HTTP request parsing and forwarding
* Concurrent client handling
* Disk-based caching
* Domain-based access control
* Network error handling
* File-based traffic logging
* Git-based version control and incremental development

## 10. AI Usage Declaration

AI tools were used as an aid during development for understanding networking concepts, debugging programming errors, reviewing implementation choices, suggesting test cases, and improving project documentation.

The final source code, design decisions, test results, and explanations should be reviewed and understood by the student before submission. AI assistance is disclosed in accordance with the assignment's requirements.
