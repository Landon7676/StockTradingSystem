#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;

#define SERVER_PORT 5432
#define MAX_LINE 256

int main(int argc, char *argv[]) {
    struct hostent *hp;
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    string host;
    int s, len;

    // Validate command-line arguments
    if (argc == 2) {
        host = argv[1];
    } else {
        cerr << "Usage: simplex-talk <host>" << std::endl;
        return 1;
    }

    // Translate host name into peer's IP address
    hp = gethostbyname(host.c_str());
    if (!hp) {
        cerr << "simplex-talk: unknown host: " << host << std::endl;
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

    cout << "Connected to server at " << host << ":" << SERVER_PORT << endl;

    // Main loop: get and send lines of text, then receive the echo
    while (true) {
        cout << "Enter command: ";
        cin.getline(buf, MAX_LINE);

        if (cin.eof()) {
            break;
        }
        
        if (strcmp(buf, "QUIT") == 0){
            cout << "200 OK" << endl;
            break;
        }

        // Send the message to the server
        len = strlen(buf) + 1;
        if (send(s, buf, len, 0) < 0) {
            perror("Send failed");
            break;
        }

        // Receive response from server
        len = recv(s, buf, sizeof(buf), 0);
        if (len > 0) {
            buf[len] = '\0'; // Null-terminate the received data

            cout << "Server Response: " << buf << endl;
        }

        else if (len == 0){
            cout << "Server disconnected.\n";
            break;
        }
        else{
            perror("Recieve failed");
            break;
        }
    }

    // Close the socket
    close(s);
    cout << "Connection closed." << endl;
    return 0;
}
