#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT 5432
#define MAX_LINE 256

int main(int argc, char *argv[]) {
    struct hostent *hp;
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    std::string host;
    int s, len;

    // Validate command-line arguments
    if (argc == 2) {
        host = argv[1];
    } else {
        std::cerr << "Usage: simplex-talk <host>" << std::endl;
        return 1;
    }

    // Translate host name into peer's IP address
    hp = gethostbyname(host.c_str());
    if (!hp) {
        std::cerr << "simplex-talk: unknown host: " << host << std::endl;
        return 1;
    }

    // Build address data structure
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);

    // Active open
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("simplex-talk: socket");
        return 1;
    }

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("simplex-talk: connect");
        close(s);
        return 1;
    }

    std::cout << "Connected to server at " << host << ":" << SERVER_PORT << std::endl;

    // Main loop: get and send lines of text, then receive the echo
    while (true) {
        std::cout << "Enter message: ";
        std::cin.getline(buf, MAX_LINE);

        if (std::cin.eof()) {
            break;
        }

        // Send the message to the server
        len = strlen(buf) + 1;
        if (send(s, buf, len, 0) < 0) {
            perror("Send failed");
            break;
        }

        // Receive the echoed message from the server
        len = recv(s, buf, sizeof(buf), 0);
        if (len > 0) {
            buf[len] = '\0'; // Null-terminate the received data
            std::cout << "Echo from server: " << buf << std::endl;
        } else if (len == 0) {
            std::cout << "Server disconnected." << std::endl;
            break;
        } else {
            perror("Receive failed");
            break;
        }
    }

    // Close the socket
    close(s);
    std::cout << "Connection closed." << std::endl;
    return 0;
}
