#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include "database.h"

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

int main() {
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    int addr_len = sizeof(sin);
    int s, new_s;

    // Initialize the database when the server starts
    std::string dbName = "trading.db";
    if (!initializeDatabase(dbName)) {
        std::cerr << "Failed to initialize database!" << std::endl;
        return 1;  // Exit if the database setup fails
    }

    std::cout << "Database initialized. Server is ready to accept connections.\n";

    // Build address data structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    // Setup passive open
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(s, MAX_PENDING) < 0) {
        perror("Listen failed");
        exit(1);
    }

    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Wait for connection, then receive, print, and echo text
    while (true) {
        if ((new_s = accept(s, (struct sockaddr *)&sin, (socklen_t *)&addr_len)) < 0) {
            perror("Accept failed");
            exit(1);
        }

        std::cout << "Client connected!" << std::endl;

        while (true) {
            memset(buf, 0, sizeof(buf)); // Clear the buffer
            int buf_len = recv(new_s, buf, sizeof(buf), 0);

            if (buf_len <= 0) {
                std::cout << "Client disconnected." << std::endl;
                break;
            }

            buf[buf_len] = '\0'; // Null-terminate the received string
            std::cout << "Received: " << buf; // Print the message received from the client

            // Echo the message back to the client
            if (send(new_s, buf, buf_len, 0) < 0) {
                perror("Send failed");
            }
        }

        close(new_s);
    }

    close(s);
    return 0;
}
