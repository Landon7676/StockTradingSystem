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

    // Initialize the database
    std::string dbName = "trading.db";
    sqlite3 *db = nullptr;

    if (!openDatabase(&db, dbName) || !initializeDatabase(dbName)) {
        std::cerr << "Failed to initialize database!" << std::endl;
        return 1;
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

    if (::bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(s, MAX_PENDING) < 0) {
        perror("Listen failed");
        exit(1);
    }

    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Main server loop
    while (true) {
        if ((new_s = accept(s, (struct sockaddr *)&sin, (socklen_t *)&addr_len)) < 0) {
            perror("Accept failed");
            exit(1);
        }

        std::cout << "Client connected!" << std::endl;

        // Process messages from this client until they disconnect
        while (true) {
            memset(buf, 0, sizeof(buf)); // Clear buffer
            int buf_len = recv(new_s, buf, sizeof(buf), 0);
            if (buf_len <= 0) {
                std::cout << "Client disconnected.\n";
                break;
            }

            buf[buf_len] = '\0'; // Null-terminate received string
            std::string input(buf);
            std::istringstream iss(input);
            std::string command;
            iss >> command;

            if (command == "ADD_USER") {
                std::string firstName, lastName, userName, password;
                double balance;

                if (iss >> firstName >> lastName >> userName >> password >> balance) {
                    bool result = addUser(firstName, lastName, userName, password, balance);
                    std::string response = result ? "ADD_USER success\n" : "ADD_USER fail\n";
                    send(new_s, response.c_str(), response.size(), 0);
                } else {
                    std::string errorMsg = "ERROR: Invalid ADD_USER format. Use: ADD_USER firstName lastName userName password balance\n";
                    send(new_s, errorMsg.c_str(), errorMsg.size(), 0);
                }
            }
            else if (command == "LIST_USERS") {
                // Implement listUsers() logic and send data back
                listUsers(); // This currently prints to stdout; modify it to return results
                std::string successMsg = "LIST_USERS executed.\n";
                send(new_s, successMsg.c_str(), successMsg.size(), 0);
            }
            else if (command == "GET_BALANCE") {
                int userId;
                if (iss >> userId) {
                    double balance = getUserBalance(userId);
                    std::string response = "BALANCE: " + std::to_string(balance) + "\n";
                    send(new_s, response.c_str(), response.size(), 0);
                } else {
                    std::string errorMsg = "ERROR: Invalid GET_BALANCE format. Use: GET_BALANCE userID\n";
                    send(new_s, errorMsg.c_str(), errorMsg.size(), 0);
                }
            }
            else if (command == "UPDATE_BALANCE") {
                int userId;
                double newBalance;
                if (iss >> userId >> newBalance) {
                    bool result = updateUserBalance(userId, newBalance);
                    std::string response = result ? "UPDATE_BALANCE success\n" : "UPDATE_BALANCE fail\n";
                    send(new_s, response.c_str(), response.size(), 0);
                } else {
                    std::string errorMsg = "ERROR: Invalid UPDATE_BALANCE format. Use: UPDATE_BALANCE userID newBalance\n";
                    send(new_s, errorMsg.c_str(), errorMsg.size(), 0);
                }
            }
            else {
                std::string errorMsg = "ERROR: Unknown command\n";
                send(new_s, errorMsg.c_str(), errorMsg.size(), 0);
            }
        }

        close(new_s);
    }

    // Close the database connection when the server shuts down
    sqlite3_close(db);
    close(s);
    return 0;
}
