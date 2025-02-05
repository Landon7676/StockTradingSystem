#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <sqlite3.h>
#include "database.h"

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

int main()
{
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    int addr_len = sizeof(sin);
    int s, new_s;
    bool shutdownRequested = false;

    // Initialize the database when the server starts
    std::string dbName = "trading.db";
    if (!initializeDatabase(dbName))
    {
        std::cerr << "Failed to initialize database!" << std::endl;
        return 1; // Exit if the database setup fails
    }

    std::cout << "Database initialized. Server is ready to accept connections.\n";

    // Build address data structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    // Setup passive open
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(s, MAX_PENDING) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Main server loop: accept new clients, then read their messages
    while (!shutdownRequested)
    {
        if ((new_s = accept(s, (struct sockaddr *)&sin, (socklen_t *)&addr_len)) < 0)
        {
            perror("Accept failed");
            exit(1);
        }

        std::cout << "Client connected!" << std::endl;

        // Process messages from this client until they disconnect
        while (true)
        {
            memset(buf, 0, sizeof(buf)); // Clear the buffer
            int buf_len = recv(new_s, buf, sizeof(buf), 0);
            if (buf_len <= 0)
            {
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

            if (command == "BUY")
            {
                // Extract required parameters
                if (!(iss >> stock_symbol >> stock_amount >> price_per_stock >> user_id))
                {
                    std::cerr << "Invalid BUY command format received: " << input << std::endl;
                    std::string errorMsg = "400 Bad Request: Invalid BUY format\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                    continue;
                }

                // Log received command
                std::cout << "s: Received: BUY " << stock_symbol << " " << stock_amount
                          << " " << price_per_stock << " " << user_id << std::endl;

                // Attempt to process the stock purchase
                if (buyStock(stock_symbol, stock_symbol, stock_amount, price_per_stock, user_id, dbName))
                {
                    // Get updated user balance and stock balance
                    double new_usd_balance = 0.0;
                    double new_stock_balance = 0.0;

                    // Query updated balances
                    sqlite3 *db;
                    sqlite3_stmt *stmt;
                    if (openDatabase(&db, dbName))
                    {
                        const char *getBalanceSQL = "SELECT usd_balance FROM Users WHERE ID = ?;";
                        sqlite3_prepare_v2(db, getBalanceSQL, -1, &stmt, nullptr);
                        sqlite3_bind_int(stmt, 1, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            new_usd_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);

                        const char *getStockSQL = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
                        sqlite3_prepare_v2(db, getStockSQL, -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 2, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW)
                        {
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
                else
                {
                    std::string errorMsg = "400 Bad Request: Transaction failed\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                }
            }
            else if (command == "SELL")
            {
                // Extract required parameters
                if (!(iss >> stock_symbol >> stock_amount >> price_per_stock >> user_id))
                {
                    std::cerr << "Invalid SELL command format received: " << input << std::endl;
                    std::string errorMsg = "400 Bad Request: Invalid SELL format\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                    continue;
                }

                // Log received command
                std::cout << "s: Received: SELL " << stock_symbol << " " << stock_amount
                          << " " << price_per_stock << " " << user_id << std::endl;

                // Attempt to process the stock sale
                if (sellStock(stock_symbol, stock_amount, price_per_stock, user_id, dbName))
                {
                    double new_usd_balance = 0.0;
                    double new_stock_balance = 0.0;

                    // Query updated balances
                    sqlite3 *db;
                    sqlite3_stmt *stmt;
                    if (openDatabase(&db, dbName))
                    {
                        const char *getBalanceSQL = "SELECT usd_balance FROM Users WHERE ID = ?;";
                        sqlite3_prepare_v2(db, getBalanceSQL, -1, &stmt, nullptr);
                        sqlite3_bind_int(stmt, 1, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            new_usd_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);

                        const char *getStockSQL = "SELECT stock_balance FROM Stocks WHERE stock_symbol = ? AND user_id = ?;";
                        sqlite3_prepare_v2(db, getStockSQL, -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, stock_symbol.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_int(stmt, 2, user_id);

                        if (sqlite3_step(stmt) == SQLITE_ROW)
                        {
                            new_stock_balance = sqlite3_column_double(stmt, 0);
                        }

                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                    }

                    std::ostringstream response;
                    response << "200 OK\nSOLD: New balance: " << new_stock_balance
                             << " " << stock_symbol << ". USD $" << new_usd_balance << "\n";
                    send(new_s, response.str().c_str(), response.str().length(), 0);
                }
                else
                {
                    std::string errorMsg = "400 Bad Request: Transaction failed\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                }
            }
            else if (command == "LIST")
            {
                // Log received command
                std::cout << "s: Received: LIST" << std::endl;

                // Prepare the response
                std::ostringstream response;

                // Initialize the SQLite database pointer and statement pointer
                sqlite3 *db;
                sqlite3_stmt *stmt;
                const char *dbName = "trading.db"; // Ensure this is your database path

                if (openDatabase(&db, dbName))
                {
                    const char *schemaQuery = "SELECT name FROM sqlite_master WHERE type='table' AND name='Stocks';";

                    // Prepare the schema query to check if 'Stocks' table exists
                    int schemaRc = sqlite3_prepare_v2(db, schemaQuery, -1, &stmt, nullptr);
                    if (schemaRc != SQLITE_OK)
                    {
                        std::cerr << "Failed to prepare schema query: " << sqlite3_errmsg(db) << std::endl;
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        return 0; // Stop further execution if schema query fails
                    }

                    if (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        std::cout << "Stocks table exists." << std::endl;
                    }
                    else
                    {
                        std::cout << "Stocks table does not exist." << std::endl;
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        return 0; // Stop if the table does not exist
                    }
                    sqlite3_finalize(stmt); // Finalize the schema check statement

                    const char *query = "SELECT ID, stock_symbol, stock_name, stock_balance, user_id FROM Stocks;";

                    // Prepare the SELECT statement to get stocks
                    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
                    if (rc != SQLITE_OK)
                    {
                        std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(db) << std::endl;
                        sqlite3_finalize(stmt);
                        sqlite3_close(db);
                        std::string errorMsg = "400 Bad Request: Unable to list stocks\n";
                        send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                        return 0; // Stop further execution if query preparation fails
                    }

                    // Start building the response
                    response << "200 OK\nThe list of stocks:\n";

                    // Iterate over the query results
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        int stock_id = sqlite3_column_int(stmt, 0);
                        const char *stock_symbol = (const char *)sqlite3_column_text(stmt, 1);
                        const char *stock_name = (const char *)sqlite3_column_text(stmt, 2);
                        double stock_balance = sqlite3_column_double(stmt, 3);
                        int user_id = sqlite3_column_int(stmt, 4);

                        // Append the data to the response
                        response << stock_id << " " << stock_symbol << " " << stock_name << " " << stock_balance << " " << user_id << "\n";
                    }

                    sqlite3_finalize(stmt); // Finalize the SELECT statement
                    sqlite3_close(db);      // Close the database connection

                    // Send the response to the client
                    send(new_s, response.str().c_str(), response.str().length(), 0);
                }
                else
                {
                    std::string errorMsg = "400 Bad Request: Unable to open database\n";
                    send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
                }
            }
            else if (command == "SHUTDOWN")
            {
                std::cout << "Received: SHUTDOWN" << std::endl;
                shutdownRequested = true;
                break;
            }
            else
            {
                std::string errorMsg = "400 Bad Request: Invalid Command\n";
                send(new_s, errorMsg.c_str(), errorMsg.length(), 0);
            }
        }

        close(new_s);
        // If shutdown is requested, break from the outer loop too
        if (shutdownRequested)
        {
            std::cout << "Shutting down the server..." << std::endl;
            break;
        }
    }

    close(s);
    return 0;
}
