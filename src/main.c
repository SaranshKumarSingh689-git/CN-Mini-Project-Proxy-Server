#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "http_parser.h"

#pragma comment(lib, "ws2_32.lib")

#define PROXY_PORT 8080
#define BACKLOG 10

int main(void)
{
    WSADATA wsaData;

    SOCKET serverSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    int clientAddressLength =
        sizeof(clientAddress);

    /*
     * Initialize Winsock.
     */

    int result = WSAStartup(
        MAKEWORD(2, 2),
        &wsaData
    );

    if (result != 0)
    {
        printf(
            "WSAStartup failed. Error: %d\n",
            result
        );

        return 1;
    }

    printf(
        "Winsock initialized successfully.\n"
    );


    /*
     * Create TCP socket.
     */

    serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (serverSocket == INVALID_SOCKET)
    {
        printf(
            "Socket creation failed. Error: %d\n",
            WSAGetLastError()
        );

        WSACleanup();

        return 1;
    }

    printf(
        "TCP socket created successfully.\n"
    );


    /*
     * Configure server address.
     */

    memset(
        &serverAddress,
        0,
        sizeof(serverAddress)
    );

    serverAddress.sin_family = AF_INET;

    serverAddress.sin_addr.s_addr =
        htonl(INADDR_ANY);

    serverAddress.sin_port =
        htons(PROXY_PORT);


    /*
     * Bind socket.
     */

    result = bind(
        serverSocket,
        (struct sockaddr *)&serverAddress,
        sizeof(serverAddress)
    );

    if (result == SOCKET_ERROR)
    {
        printf(
            "Bind failed. Error: %d\n",
            WSAGetLastError()
        );

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    printf(
        "Proxy bound to port %d.\n",
        PROXY_PORT
    );


    /*
     * Start listening.
     */

    result = listen(
        serverSocket,
        BACKLOG
    );

    if (result == SOCKET_ERROR)
    {
        printf(
            "Listen failed. Error: %d\n",
            WSAGetLastError()
        );

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    printf(
        "Proxy server is listening...\n"
    );

    printf(
        "Waiting for client connections...\n\n"
    );


    /*
     * Accept clients.
     */

    while (1)
    {
        clientSocket = accept(
            serverSocket,
            (struct sockaddr *)&clientAddress,
            &clientAddressLength
        );

        if (clientSocket == INVALID_SOCKET)
        {
            printf(
                "Accept failed. Error: %d\n",
                WSAGetLastError()
            );

            continue;
        }


        printf(
            "Client connected: %s:%d\n",
            inet_ntoa(
                clientAddress.sin_addr
            ),
            ntohs(
                clientAddress.sin_port
            )
        );


        /*
         * Receive HTTP request.
         */

        char requestBuffer[MAX_HTTP_REQUEST];

        memset(
            requestBuffer,
            0,
            sizeof(requestBuffer)
        );

        int bytesReceived = recv(
            clientSocket,
            requestBuffer,
            sizeof(requestBuffer) - 1,
            0
        );

        if (bytesReceived == SOCKET_ERROR)
        {
            printf(
                "Receive failed. Error: %d\n",
                WSAGetLastError()
            );

            closesocket(clientSocket);

            continue;
        }

        if (bytesReceived == 0)
        {
            printf(
                "Client closed connection.\n"
            );

            closesocket(clientSocket);

            continue;
        }

        requestBuffer[bytesReceived] = '\0';


        printf(
            "\n----- HTTP REQUEST -----\n"
        );

        printf(
            "%s\n",
            requestBuffer
        );

        printf(
            "------------------------\n"
        );


        /*
         * Parse HTTP request.
         */

        HttpRequest parsedRequest;

        int parseResult =
            parse_http_request(
                requestBuffer,
                &parsedRequest
            );


        if (parseResult == -1)
        {
            const char *response =
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 16\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Bad HTTP request";

            send(
                clientSocket,
                response,
                (int)strlen(response),
                0
            );

            closesocket(clientSocket);

            continue;
        }


        if (parseResult == -2)
        {
            const char *response =
                "HTTP/1.1 501 Not Implemented\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 22\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Method not supported";

            send(
                clientSocket,
                response,
                (int)strlen(response),
                0
            );

            closesocket(clientSocket);

            continue;
        }


        /*
         * Display parsed request.
         */

        printf(
            "\n===== PARSED REQUEST =====\n"
        );

        printf(
            "Method : %s\n",
            parsedRequest.method
        );

        printf(
            "Host   : %s\n",
            parsedRequest.host
        );

        printf(
            "Port   : %d\n",
            parsedRequest.port
        );

        printf(
            "Path   : %s\n",
            parsedRequest.path
        );

        printf(
            "==========================\n\n"
        );
        /*
         * Temporary response.
         *
         * Actual forwarding will be implemented
         * in the next stage.
         */
        const char *body =
            "HTTP request parsed successfully.\n";

        char response[512];

        snprintf(
            response,
            sizeof(response),

            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",

            (int)strlen(body),
            body
        );
        send(
            clientSocket,
            response,
            (int)strlen(response),
            0
        );
        closesocket(clientSocket);

        printf(
            "Client connection closed.\n\n"
        );
    }
    /*
     * Cleanup.
     */
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}