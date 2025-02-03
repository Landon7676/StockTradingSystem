#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
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
            std::string command, stock_symbol;
            double stock_amount, price_per_stock;
            int user_id;
            iss >> command;

            if (command == "BUY") {
                // Extract required parameters
                if (!(iss >> stock_symbol >> stock_amount >> price_per_stock >> user_id)) {
                    std::cerr << "Invalid BUY command format received: " << input << std::endl;
                    std::string errorMsg = "400 Bad Request: Invalid BUY format\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                    continue;
                }

                // Log received command
                std::cout << "s: Received: BUY " << stock_symbol << " " << stock_amount 
                          << " " << price_per_stock << " " << user_id << std::endl;

                // Attempt to process the stock purchase
                if (buyStock(stock_symbol, stock_symbol, stock_amount, price_per_stock, user_id, dbName)) {
                    // Get updated user balance and stock balance
                    double new_usd_balance = 0.0;
                    double new_stock_balance = 0.0;

                    // Query updated balances
                    sqlite3 *db;
                    sqlite3_stmt *stmt;
                    if (openDatabase(&db, dbName)) {
                        const char *getBalanceSQL = "SELECT usd_balance FROM Users WHERE ID = ?;";
                        sqlite3_prepare_v2(db, getBalanceSQL, -1, &stmt, nullptr);
                        sqlite3_bind_int(stmt, 1, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            new_usd_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);

                        const char *getStockSQL = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
                        sqlite3_prepare_v2(db, getStockSQL, -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 2, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            new_stock_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                    }

                    std::ostringstream response;
                    response << "200 OK\nBOUGHT: New balance: " << new_stock_balance 
                             << " " << stock_symbol << ". USD balance $" << new_usd_balance << "\n";
                    send(new_s, response.str().c_str(), response.str().length(), 0);
                } 
                else {
                    std::string errorMsg = "400 Bad Request: Transaction failed\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                }
            } 
            else if (command == "SELL") {
                // Extract required parameters
                if (!(iss >> stock_symbol >> stock_amount >> price_per_stock >> user_id)) {
                    std::cerr << "Invalid SELL command format received: " << input << std::endl;
                    std::string errorMsg = "400 Bad Request: Invalid SELL format\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                    continue;
                }

                // Log received command
                std::cout << "s: Received: SELL " << stock_symbol << " " << stock_amount 
                          << " " << price_per_stock << " " << user_id << std::endl;

                // Attempt to process the stock sale
                if (sellStock(stock_symbol, stock_amount, price_per_stock, user_id, dbName)) {
                    double new_usd_balance = 0.0;
                    double new_stock_balance = 0.0;

                    // Query updated balances
                    sqlite3 *db;
                    sqlite3_stmt *stmt;
                    if (openDatabase(&db, dbName)) {
                        const char *getBalanceSQL = "SELECT usd_balance FROM Users WHERE ID = ?;";
                        sqlite3_prepare_v2(db, getBalanceSQL, -1, &stmt, nullptr);
                        sqlite3_bind_int(stmt, 1, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            new_usd_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);

                        const char *getStockSQL = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
                        sqlite3_prepare_v2(db, getStockSQL, -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 2, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            new_stock_balance = sqlite3_column_double(stmt, 0);
                        }
                        
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                    }

                    std::ostringstream response;
                    response << "200 OK\nSOLD: New balance: " << new_stock_balance 
                             << " " << stock_symbol << ". USD $" << new_usd_balance << "\n";
                    send(new_s, response.str().c_str(), response.str().length(), 0);
                } else {
                    std::string errorMsg = "400 Bad Request: Transaction failed\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                }
            }
            else {
                std::string unknownCmd = "400 Bad Request: Unknown Command\n";
                send(new_s, unknownCmd.c_str(), unknownCmd.length(), 0);
            }
        }

        close(new_s);
    }

    close(s);
    return 0;
}
