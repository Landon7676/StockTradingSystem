#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <sqlite3.h>
#include <pthread.h>
#include "database.h"
#include <map>

#define SERVER_PORT 5431
#define MAX_PENDING 5
#define MAX_LINE 256

static bool shutdownRequested = false;
static const std::string dbName = "trading.db";
static std::map<int,int> socketToUserId;
static int listeningSock;

/** 
*Function used to handle a single client's connection and also moved the per-client while loop here,
*so each client is handled in parallel by its own thread
**/
void* handle_single_thread(void* client_socket){
    // Convert the generic pointer to an int pointer, then copy and free it
    int sock = *(int*)client_socket;
    free(client_socket);

    char buf[MAX_LINE];

	// Debug output to check if client is truly multithreaded
	std::cout << "[DEBUG] Thread ID: " << pthread_self() << " handling client on socket " << sock << std::endl;

	// Process messages from this client until they disconnect or issue SHUTDOWN
    while (!shutdownRequested)
    {
		memset(buf, 0, sizeof(buf)); // Clear the buffer
		int buf_len = recv(sock, buf, sizeof(buf), 0);
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
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			// Check for negative numbers in the BUY command parameters.
			// If any negative value is provided, reject the command.
			if (stock_amount < 0 || price_per_stock < 0 || user_id < 0)
			{
				std::cerr << "Invalid BUY command: Negative values are not allowed (" << input << ")" << std::endl;
				std::string errorMsg = "400 Bad Request: Negative values are not permitted in BUY command\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
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
				send(sock, response.str().c_str(), response.str().length(), 0);
			}
			else
			{
				std::string errorMsg = "400 Bad Request: Transaction failed\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
			}
		}
		else if (command == "SELL")
		{
			// Extract required parameters
			if (!(iss >> stock_symbol >> stock_amount >> price_per_stock >> user_id))
			{
				std::cerr << "Invalid SELL command format received: " << input << std::endl;
				std::string errorMsg = "400 Bad Request: Invalid SELL format\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			// Check for negative numbers in the SELL command parameters.
			if (stock_amount < 0 || price_per_stock < 0 || user_id < 0)
			{
				std::cerr << "Invalid SELL command: Negative values are not allowed (" << input << ")" << std::endl;
				std::string errorMsg = "400 Bad Request: Negative values are not permitted in SELL command\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
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
				send(sock, response.str().c_str(), response.str().length(), 0);
			}
			else
			{
				std::string errorMsg = "400 Bad Request: Transaction failed\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
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
					send(sock, errorMsg.c_str(), errorMsg.length(), 0);
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
				send(sock, response.str().c_str(), response.str().length(), 0);
			}
			else
			{
				std::string errorMsg = "400 Bad Request: Unable to open database\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
			}
		}
		else if (command == "BALANCE")
		{
			std::cout << "s: Received: BALANCE" << std::endl;

			int user_id = 1; // Always show balance for user 1
			std::string first_name, last_name;
			double usd_balance;

			if (getUserBalance(user_id, first_name, last_name, usd_balance, dbName))
			{
				std::ostringstream response;
				response << "200 OK\n"
							<< "Balance for user " << first_name << " " << last_name
							<< ": $" << usd_balance << "\n";
				std::string responseStr = response.str();

				std::cout << "Sending response: " << responseStr; // Debug log
				send(sock, responseStr.c_str(), responseStr.length(), 0);
			}
			else
			{
				std::string errorMsg = "404 Not Found\nUser with ID " + std::to_string(user_id) + " does not exist.\n";
				std::cout << "Sending error response: " << errorMsg; // Debug log
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
			}
		}

		else if (command == "LOGIN") {
			// Parse out the username and password from input
			std::string userName, password;
			
			if(!(iss >> userName >> password))
			{
				// Client did not provide enough arguments
				std::string errMsg = "400 Bad Request: Invalid login format. Usage: LOGIN <username> <password>\n";
				send(sock, errMsg.c_str(), errMsg.length(), 0);
				continue;
			}

			// Now check credentials
			int dbUserId = -1; // Will be set within method
			if(!checkCredentials(userName, password, dbUserId, dbName))
			{
				std::string errMsg = "403 Wrong Username or Password\n";
				send(sock, errMsg.c_str(), errMsg.length(), 0);
				continue;
			}

			socketToUserId[sock] = dbUserId;

			std::string successMsg = "200 Ok: Login successful\n";
			send(sock, successMsg.c_str(), successMsg.length(), 0);

			std::cout << "[INFO] Socket " << sock << " successfully logged in as user ID " << dbUserId << std::endl;
		}

		else if (command == "SHUTDOWN")
		{
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if(userIterator == socketToUserId.end())
			{
				std:: string errorMsg = "403 Forbidden: Not logged in. Only the root user is allowed to shutdown the server\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			// Get current userID from the map
			int currUserID = userIterator->second;

			// Check if that userID is the root user
			if(currUserID == 1)
			{
				std::cout << "Received: SHUTDOWN" << std::endl;
				shutdownRequested = true;
				break;
			}
			else if(currUserID > 1)
			{
				std:: string errorMsg = "403 Forbidden: Only the root user is allowed to shutdown the server\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}
		}

		else if(command.empty())
		{
			continue;
		}

		else
		{
			// Invalid command
			std::string errorMsg = "400 Bad Request: Invalid Command\n";
			send(sock, errorMsg.c_str(), errorMsg.length(), 0);
		}	
    }
	
	// Close the client socket
}

int main()
{
    struct sockaddr_in sin;
    // Initialize the database when the server starts
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

    // Create the socket
	int s = socket(AF_INET, SOCK_STREAM, 0);
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(1);
    }

	// Bind the socket
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("Bind failed");
        exit(1);
    }

	// Listen for incoming connections
    if (listen(s, MAX_PENDING) < 0)
    {
        perror("Listen failed");
        exit(1);
    }

    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;

    // Main server loop: accept new clients until SHUTDOWN is requested
	while (!shutdownRequested){

		// Create seperate socket for each client
		struct sockaddr_in client_address;
		socklen_t addr_len = sizeof(client_address);

		int *new_sock_ptr = new int;
		*new_sock_ptr = accept(s, (struct sockaddr *)&client_address, &addr_len);

		if (*new_sock_ptr < 0)
        {
            // If SHUTDOWN is triggered while in accept, we often get an error
            if (shutdownRequested) break;
            perror("Accept failed");
            delete new_sock_ptr;
            continue;
        }

		std ::cout << "Client connected!" << std::endl;

		// Create new thread to handle this client
		pthread_t threadId;
		if (pthread_create(&threadId, nullptr, handle_single_thread, new_sock_ptr) != 0)
		{
			std::cerr << "Error creating thread for new client." << std::endl;
			close(*new_sock_ptr);
			delete new_sock_ptr;
		}
		else
		{
			// Detach the thread so its resources are reclaimed
			pthread_detach(threadId);
		}
	}
    close(s);
    return 0;
}
