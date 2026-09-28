#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

using namespace std;

// keeps calling send() until all bytes are sent
bool sendAll(int destinationSocket, const char* data, int length) { 
    int totalSent = 0;

    while (totalSent < length) {
        int bytesSent = send(
            destinationSocket,
            data + totalSent,   // starting position in buffer
            length - totalSent, // number of bytes left to send
            0
        );

        if (bytesSent <= 0) {
            return false;
        }

        totalSent += bytesSent;
    }

    return true;
}

int main() {
    cout << "Proxy starting..." << endl;

    // create listening socket
    int listening = socket(AF_INET, SOCK_STREAM, 0);

    if (listening == -1) {  // in case something went wrong
        cerr << "Couldnt create a socket";
        return -1;
    }

    // bind listener socket to an IP/port
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54001);
    inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

    if (bind(listening, (sockaddr*)&hint, sizeof(hint)) == -1) {
        cerr << "Couldnt bind socket to IP/port" << endl;
        close(listening);
        return -2;
    }

    // mark as listener 
    if (listen(listening, SOMAXCONN) == -1) {
        cerr << "Couldnt listen for connections";
        close(listening);   // no use for listener socket then
        return -3;
    }
    else {
        cout << "Listening for connections..." << endl;
    }

    // create client socket (minecraft player)
    sockaddr_in client; 
    socklen_t clientSize = sizeof(client);

    // listener accept/establish connection with client
    int clientSocket = accept(listening, (sockaddr*)&client, &clientSize); 

    if (clientSocket == -1) {
        cerr << "Listener couldnt connect with client socket";
        close(listening);
        return -4;
    }

    // display host info 
    char host[NI_MAXHOST];  // buffer
    char svc[NI_MAXSERV];  
    memset(host, 0, NI_MAXHOST);    // cleaning buffers
    memset(svc, 0, NI_MAXSERV);

    int result = getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, svc, NI_MAXSERV, 0); // getting the name of the host

    if (result == 0) {  // getnameinfo() returns 0 on success
        cout <<  host << " connected on " << svc << endl;
    }
    else {
        // do it manually if it fails
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        cout << host << " connected on " << ntohs(client.sin_port) << endl;
    }

    // create upstream server socket (minecraft server)
    int upstreamSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (upstreamSocket == -1) {
        cerr << "Couldnt create upstream socket" << endl;
        close(listening);
        close(clientSocket);
        return -5;
    }

    // hint structure for destination (server) that we're connecting to 
    sockaddr_in upstream;
    upstream.sin_family = AF_INET;
    int upstream_port = 54000;
    upstream.sin_port = htons(upstream_port);   // server port runs on 54000, see main.cpp
    inet_pton(AF_INET, "127.0.0.1", &upstream.sin_addr);    // Use 127.0.0.1 because connect() needs one specific destination server. 0.0.0.0 is a wildcard

    // connect to upstream
    if (connect(upstreamSocket, (sockaddr*)&upstream, sizeof(upstream)) == -1) {
        cerr << "Couldnt connect to upstream server" << endl;
        close(listening);
        close(clientSocket);
        close(upstreamSocket);
        return -6;
    }
    else {
        cout << "Connected to upstream on port " << upstream_port << endl;
    }

    // receive message
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));  // clean buffer

    while(true) {   // infinite loop; keep receiving bytes until client disconnects
        int bytesRecv = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytesRecv == -1) {
            cerr << "Couldnt receive bytes from client" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -7;
        }

        if (bytesRecv == 0) {
            cerr << "Client disconnected" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -8;
        }

        // display message
        cout << "Received: " << string(buffer, 0, bytesRecv) << endl;

        // forward/send bytes to upstream (server)
        if (!sendAll(upstreamSocket, buffer, bytesRecv)) {
            cerr << "Could not forward bytes to upstream" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -9;
        }
        else {
            cout << "Forwarded " << bytesRecv << " bytes upstream" << endl;
        }

        // wait for server response, echo response
        int upstreamBytesRecv = recv(upstreamSocket, buffer, sizeof(buffer), 0);

        if (upstreamBytesRecv == -1) {
            cerr << "Couldnt receive upstream server response" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -10;
        }

        if (upstreamBytesRecv == 0) {
            cerr << "Upstream server disconnected" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -11;
        }

        // display response
        cout << "UPSTREAM SERVER: " << string(buffer, 0, upstreamBytesRecv) << endl;

        // echo server response back to client
        int bytesSentToClient = send(clientSocket, buffer, upstreamBytesRecv, 0);

        if (bytesSentToClient == -1) {
            cerr << "Couldnt echo response back to client" << endl;
            close(listening);
            close(clientSocket);
            close(upstreamSocket);
            return -12;
        }
        else {
            cout << "Echoed " << bytesSentToClient << " bytes back to client" << endl;
        }
        
    }

}