// ClientTester.cpp

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory.h>
#include <iostream>
#include <fstream>

#define MAX_CONNECTIONS 10
#define MAX_BUFFER_LENGTH 325

std::ofstream *logFile;
int testerSocketFD, commandSocketFD;
struct sockaddr_in testerAddr, cliAddr;
int port = 8080;
char buff[MAX_BUFFER_LENGTH];


// create socket
void createSocket()
{
    testerSocketFD = socket(AF_INET , SOCK_STREAM , 0);
    if(testerSocketFD < 0){
        std::cout << "CREATE SOCKET ERROR" << std::endl;
    }
    std::cout << "create socket success" << std::endl;
}

// bind socket
void bindSocket()
{
    if(bind(testerSocketFD, reinterpret_cast<struct sockaddr *>(&testerAddr), sizeof(sockaddr_in)) < 0)
    {
        std::cout << "BIND SOCKET ERROR" << std::endl;
    }
    std::cout << "socket bind success" << std::endl;
}

// listen on socket
void listenSocket()
{
    listen(testerSocketFD, MAX_CONNECTIONS);
    std::cout << "listening..." << std::endl;
}

// test connectivity
void testConnectivity()
{

}

// test server response
void testServerResponse()
{}

int main ()
{
    logFile = new std::ofstream;
    (*logFile).open("client_tester.log", std::ios_base::app);


    testerAddr.sin_family = AF_INET;
    testerAddr.sin_port = (uint16_t) port;
    socklen_t addressLength = sizeof(sockaddr_in);
    memset(&testerAddr, 0, addressLength);

    // create bind and listen on socket
    createSocket();
    bindSocket();
    listenSocket();

    // get few commands and print them to log file and stdout?
    std::cout << "waiting to accept connection..." << std::endl;
    commandSocketFD = accept(testerSocketFD, reinterpret_cast<struct sockaddr *>(&cliAddr), &addressLength);
    ssize_t readSize = read(commandSocketFD, buff, MAX_BUFFER_LENGTH);
    std::cout << "buff: " << buff << std::endl;

    // send responses

}