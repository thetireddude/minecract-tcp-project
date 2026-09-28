#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>

using namespace std;

int main() {
    cout << "This is running" << endl;

    // Create client socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {   // in case something goes wrong
        cerr << "Couldnt create socket" << endl;
        return 1;
    }

    // hint structure for SERVER that we're connecting to
    int port = 54000;
    string ipAddress = "127.0.0.1";

    sockaddr_in hint;   // convention to name as hint
    hint.sin_family =  AF_INET;
    hint.sin_port = htons(port);    // htons -> host to network short conversion
    inet_pton(AF_INET, ipAddress.c_str(), &hint.sin_addr);  // c_str() -> convert string type to null terminated char array pointer

    // connect sock to server 
    int connectRes = connect(sock, (sockaddr*)&hint, sizeof(hint));

    if (connectRes == -1) { // in case something goes wrong
        cerr << "Couldnt connect socket to server" << endl;
        return 2;
    }

    // send user input to server, echo response 
    char buff[4096];
    string userInput; 

    while(true) {
        // get  user input message
        cout << "> ";
        getline(cin, userInput);

        if (userInput == "exit") {
            break;
        }

        // send to server
        int sendRes = send(sock, userInput.c_str(), userInput.size()+1, 0);

        if (sendRes == -1) {
            cerr << "Couldnt send to server" << endl;
            continue;
        }

        // wait for response
        memset(buff, 0, sizeof(buff));  // clean buffer
        int bytesRecv = recv(sock, buff, sizeof(buff), 0);

        if (bytesRecv == -1) {
            cerr << "Could not get response from server" << endl;
        }
        else {
            // display response
            cout << "SERVER: " << string(buff, 0, bytesRecv) << endl;
        }
    }

    // close socket
    close(sock);


    return 0;
}