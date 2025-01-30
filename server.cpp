#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include "database.h"
using namespace std;

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

     // Main server loop: accept new clients, then read their messages
    while (true) {
        if ((new_s = accept(s, (struct sockaddr *)&sin, (socklen_t *)&addr_len)) < 0) {
            perror("Accept failed");
            exit(1);
        }

        std::cout << "Client connected!" << std::endl;

        // Process messages from this client until they disconnect
        while (true) {
            memset(buf, 0, sizeof(buf)); // Clear the buffer
            int buf_len = recv(new_s, buf, sizeof(buf), 0);
            if (buf_len <= 0) {
                std::cout << "Client disconnected.\n";
                break;
            }

            // Null-terminate the received string to safely use it
            buf[buf_len] = '\0';

            // Convert the received C-string into a std::string for easier parsing
            std::string input(buf);

            // Parse the command
            std::istringstream iss(input);
            std::string command;
            iss >> command; // The first token is assumed to be the command name

            if (command == "ADD_USER") {
                // We expect 5 arguments after ADD_USER:
                // first_name, last_name, user_name, password, balance
                std::string firstName, lastName, userName, password;
                double balance = 0.0;

                iss >> firstName >> lastName >> userName >> password >> balance;

                // Check if we actually got all the required arguments
                if (firstName.empty() || lastName.empty() || 
                    userName.empty()  || password.empty() ||
                    iss.fail()) 
                {
                    std::string errorMsg = "ERROR: Invalid ADD_USER command format.\n"
                                           "Expected: ADD_USER firstName lastName userName password balance\n";
                    send(new_s, errorMsg.c_str(), errorMsg.size(), 0);
                } else {
                    // Call the addUser database function
                    bool result = addUser(firstName, lastName, userName, password, balance);
                    if (result) {
                        std::string successMsg = "ADD_USER success: User added.\n";
                        send(new_s, successMsg.c_str(), successMsg.size(), 0);
                    } else {
                        std::string failMsg = "ADD_USER fail: Could not add user to DB.\n";
                        send(new_s, failMsg.c_str(), failMsg.size(), 0);
                    }
                }
            }
            else {
                // If it's not a recognized command, just echo it back (or handle differently)
                std::cout << "Received unrecognized command: " << input << std::endl;
                if (send(new_s, buf, buf_len, 0) < 0) {
                    perror("Send failed");
                }
            }
        }

        close(new_s);
    }

    close(s);
    return 0;
}
