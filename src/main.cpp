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

    // create a socket 
    int listening = socket(AF_INET, SOCK_STREAM, 0);    //  returns an int which is the fd of the socket
    if (listening == -1) {  // in case something went wrong
        cerr << "Couldnt create a socket";
        return -1;
    }

    // bind socket to an IP and port
    sockaddr_in hint;   // called hint by convention 
    hint.sin_family = AF_INET; 
    hint.sin_port = htons(54000);   // converting machine understood port number to standard format network understands.
    inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);  // 0.0.0.0 means we choose any IP, stored in hint.sin_addr
    
    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == -1) {  // have to cast sockaddr_in to sock_addr
        cerr << "Couldnt bind to IP/port";
        return -2;  // arbitrarily returning error code -2 so we can distinguish errors
    }

    // mark socket for listening
    if (listen(listening, SOMAXCONN) == -1) {
        cerr << "Couldnt listen for connections...";
        return -3;
    }
    else {
        cout << "Listening for connections..." << endl;
    }

    // accepting a call; creating a client socket
    sockaddr_in client; 
    socklen_t clientSize = sizeof(client);
    char host[NI_MAXHOST];  // buffer
    char svc[NI_MAXSERV];   // buffer

    int clientSocket = accept(listening, (sockaddr*)&client, &clientSize); // accept/establish connection with client

    if (clientSocket == -1) {
        cerr << "Problem with client socket connection";
        return -4;
    }

    close(listening);   // prevents listener from accepting further client connections


    memset(host, 0, NI_MAXHOST);    // cleaning buffers
    memset(svc, 0, NI_MAXSERV);
 
    int result = getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, svc, NI_MAXSERV, 0); // getting the name of the host

    if (result == 0) {
        cout <<  host << " connected on " << svc << endl;
    }
    else {
        // do it manually if it fails
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        cout << host << " connected on " << ntohs(client.sin_port) << endl;
    }

    // display message while receiving, echo message
    char buff[4096];

    while (true) {  // infinite loop to keep receiving bytes until client disconnect
        // clear buffer
        memset(buff, 0, sizeof(buff)); 

        // wait for message
        int bytesRecv = recv(clientSocket, &buff, sizeof(buff), 0);  

        if (bytesRecv == -1) {
            cerr << "There was a connection issue" << endl;
            break;
        }

        if (bytesRecv == 0) {
            cerr << "The client disconnected" << endl;
            break;

        }

        // display message
        cout << "Received: " << string(buff, 0, bytesRecv) << endl;

        // resend message
        send(clientSocket, buff, bytesRecv+1, 0); // will be used to relay bytes over to another server later
    }

    // close socket
    close(clientSocket);    // no need to keep client open after receiving bytes


    return 0;
}