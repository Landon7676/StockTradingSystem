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

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

static bool shutdownRequested = false;
static const std::string dbName = "trading.db";

/** 
*Function used to handle a single client's connection and also moved the per-client while loop here,
*so each client is handled in parallel by its own thread
**/
void* handle_single_thread(void* client_socket){
    // Convert the generic pointer to an int pointer, then copy and free it
    int sock = *(int*)client_socket;
    free(client_socket);

    char buf[MAX_LINE];

    while (!shutdownRequested)
    {

    }
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
		if (pthread_create(&threadId, nullptr, handle_client, new_sock_ptr) != 0)
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
