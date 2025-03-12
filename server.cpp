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
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <cerrno>

#define SERVER_PORT 5431
#define MAX_PENDING 5
#define MAX_LINE 256

static bool shutdownRequested = false;
static const std::string dbName = "trading.db";
static std::map<int, int> socketToUserId;
std::map<int, std::string> socketToIP;

/**
 *Function used to handle a single client's connection and also moved the per-client while loop here,
 *so each client is handled in parallel by its own thread
 **/
void *handle_single_thread(void *client_socket)
{
	// Convert the generic pointer to an int pointer, then copy and free it
	int sock = *(int *)client_socket;
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
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}
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
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}
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
		else if (command == "DEPOSIT")
		{
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			double amount;
			if (!(iss >> amount) || amount <= 0)
			{
				std::string errorMsg = "400 Bad Request: Invalid deposit amount\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			int user_id = userIterator->second;
			if (depositAmount(user_id, amount, dbName))
			{
				std::ostringstream response;
				response << "200 OK\nDeposit successful. New balance $" << getUserBalance(user_id, dbName) << "\n";
				send(sock, response.str().c_str(), response.str().length(), 0);
			}
			else
			{
				std::string errorMsg = "400 Bad Request: Deposit failed\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
			}
		}
		else if (command == "WHO")
		{
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end() || userIterator->second != 1) // Ensure only root can run this
			{
				std::string errorMsg = "403 Forbidden: Only the root user can execute this command\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			std::ostringstream response;
			response << "200 OK\nActive users:\n"
					 << getActiveUsers();
			send(sock, response.str().c_str(), response.str().length(), 0);
		}
		else if (command == "LOOKUP")
		{
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			std::string ticker;
			if (!(iss >> ticker))
			{
				std::string errorMsg = "400 Bad Request: Missing stock ticker\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			int user_id = userIterator->second;
			std::string stockInfo = lookupStock(user_id, ticker, dbName);

			if (!stockInfo.empty())
			{
				std::ostringstream response;
				response << "200 OK\nFound match:\n"
						 << stockInfo;
				send(sock, response.str().c_str(), response.str().length(), 0);
			}
			else
			{
				std::string errorMsg = "404 Your search did not match any records.\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
			}
		}

		else if (command == "LIST")
		{
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			// Determine the current user ID
			int currentUserId = userIterator->second;
			std::cout << "s: Received: LIST" << std::endl;

			// Prepare the response
			std::ostringstream response;

			// Open the database
			sqlite3 *db;
			sqlite3_stmt *stmt;
			if (openDatabase(&db, dbName))
			{
				// Make sure the Stocks table exists
				const char *schemaQuery = "SELECT name FROM sqlite_master WHERE type='table' AND name='Stocks';";
				int schemaRc = sqlite3_prepare_v2(db, schemaQuery, -1, &stmt, nullptr);
				if (schemaRc != SQLITE_OK)
				{
					std::cerr << "Failed to prepare schema query: " << sqlite3_errmsg(db) << std::endl;
					sqlite3_finalize(stmt);
					sqlite3_close(db);
					continue;
				}

				if (sqlite3_step(stmt) != SQLITE_ROW)
				{
					std::cout << "Stocks table does not exist." << std::endl;
					sqlite3_finalize(stmt);
					sqlite3_close(db);
					continue;
				}
				sqlite3_finalize(stmt);

				// Build the SELECT statement depending on whether the user is root or not
				std::string query;
				if (currentUserId == 1)
				{
					// Root user sees ALL stocks
					query = "SELECT ID, stock_symbol, stock_name, stock_balance, user_id FROM Stocks;";
				}
				else
				{
					// Non-root user sees ONLY their own stocks
					query = "SELECT ID, stock_symbol, stock_name, stock_balance, user_id "
							"FROM Stocks WHERE user_id = ?;";
				}

				// Prepare the query
				int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
				if (rc != SQLITE_OK)
				{
					std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(db) << std::endl;
					sqlite3_finalize(stmt);
					sqlite3_close(db);
					std::string errorMsg = "400 Bad Request: Unable to list stocks\n";
					send(sock, errorMsg.c_str(), errorMsg.length(), 0);
					continue;
				}

				// If not root, bind the user’s ID
				if (currentUserId != 1)
				{
					sqlite3_bind_int(stmt, 1, currentUserId);
				}

				// Build the response header
				response << "200 OK\nThe list of stocks:\n";

				// Read each row from the query result
				while (sqlite3_step(stmt) == SQLITE_ROW)
				{
					int stock_id = sqlite3_column_int(stmt, 0);
					const char *stock_symbol = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
					const char *stock_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
					double stock_balance = sqlite3_column_double(stmt, 3);
					int user_id = sqlite3_column_int(stmt, 4);

					response << stock_id << " "
							 << (stock_symbol ? stock_symbol : "") << " "
							 << (stock_name ? stock_name : "") << " "
							 << stock_balance << " "
							 << user_id << "\n";
				}

				// Finalize and close
				sqlite3_finalize(stmt);
				sqlite3_close(db);

				// Send the response back to the client
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
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "401 Forbidden: You must login first\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}
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

		else if (command == "LOGIN")
		{
			// Parse out the username and password from input
			std::string userName, password;

			if (!(iss >> userName >> password))
			{
				// Client did not provide enough arguments
				std::string errMsg = "400 Bad Request: Invalid login format. Usage: LOGIN <username> <password>\n";
				send(sock, errMsg.c_str(), errMsg.length(), 0);
				continue;
			}

			// Now check credentials
			int dbUserId = -1; // Will be set within method
			if (!checkCredentials(userName, password, dbUserId, dbName))
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

		else if (command == "LOGOUT")
		{
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "403 Forbidden: Not logged in.\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			else
			{
				socketToUserId.erase(sock);
				std::string successMsg = "200 Ok: Logout successful\n";
				send(sock, successMsg.c_str(), successMsg.length(), 0);
			}
		}

		else if (command == "SHUTDOWN")
		{
			// Check if the user is logged in
			auto userIterator = socketToUserId.find(sock);
			if (userIterator == socketToUserId.end())
			{
				std::string errorMsg = "403 Forbidden: Not logged in. Only the root user is allowed to shutdown the server\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}

			// Get current userID from the map
			int currUserID = userIterator->second;

			// Check if that userID is the root user
			if (currUserID == 1)
			{
				std::cout << "Received: SHUTDOWN" << std::endl;
				shutdownRequested = true;
				break;
			}
			else if (currUserID > 1)
			{
				std::string errorMsg = "403 Forbidden: Only the root user is allowed to shutdown the server\n";
				send(sock, errorMsg.c_str(), errorMsg.length(), 0);
				continue;
			}
		}

		else
		{
			// Invalid command
			std::string errorMsg = "400 Bad Request: Invalid Command\n";
			send(sock, errorMsg.c_str(), errorMsg.length(), 0);
		}
	}

	// Close the client socket
	close(sock);
	return nullptr;
}
std::string getActiveUsers()
{
	std::ostringstream users;
    for (const auto &entry : socketToUserId)
    {
        int sock = entry.first;
        int user_id = entry.second;
        std::string ip_address = socketToIP[sock]; // Get IP from new map

        users << "UserID: " << user_id << " (IP: " << ip_address << ")\n";
    }
    return users.str();
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
	while (!shutdownRequested)
	{

		fd_set readfds;		 // Declares a set of file descriptors we want to watch for read-readiness
		FD_ZERO(&readfds);	 // Initializes the set to be empty
		FD_SET(s, &readfds); // Adds our listening socket to this set
		int maxFd = s;

		struct timeval tv; // We set up a timeval struct to a 1-second timeout
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		int ready = select(maxFd + 1, &readfds, NULL, NULL, &tv);

		if (ready < 0)
		{
			if (shutdownRequested)
				break;
			perror("select failed");
			continue;
		}
		else if (ready == 0)
		{
			// Timeout; no new connections in this 1-second window
			continue;
		}

		// If we get here, the listening socket is readable
		if (FD_ISSET(s, &readfds))
		{
			struct sockaddr_in client_address;
			socklen_t addr_len = sizeof(client_address);
			int *new_sock_ptr = new int;
			*new_sock_ptr = accept(s, (struct sockaddr *)&client_address, &addr_len);

			if (*new_sock_ptr >= 0)
			{
				// Convert IP address to string
				char client_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);

				// Store socket-to-IP mapping
				socketToIP[*new_sock_ptr] = std::string(client_ip);

				std::cout << "[INFO] Client connected: " << client_ip << std::endl;
			}

			if (*new_sock_ptr < 0)
			{
				if (shutdownRequested)
				{
					delete new_sock_ptr;
					break;
				}
				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					perror("accept failed");
				}
				delete new_sock_ptr;
				continue;
			}

			// If accept succeeded we have a valid connection socket in the pointer
			// We create a new thread to handle the client
			pthread_t threadId;
			if (pthread_create(&threadId, nullptr, handle_single_thread, new_sock_ptr) != 0)
			{
				std::cerr << "Error creating thread for new client." << std::endl;
				close(*new_sock_ptr);
				delete new_sock_ptr;
			}
			else
			{
				pthread_detach(threadId);
			}
		}
	}
	close(s);
	return 0;
}
