//
// Created by asaf on 6/7/16.
//

#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <fstream>
#include <ctime>
#include <deque>
#include <algorithm>
#include <mutex>

#define NUMBER_OF_ARGS 4
#define MAX_BUFFER_LENGTH 325

// global variables
std::ofstream* logFile;
int port;
struct hostent *serverIP;
struct sockaddr_in serverAddr;
std::string clientName;
std::string msgToServer;
char buff[MAX_BUFFER_LENGTH];
int clientSocketDesc;
bool registered;
bool dontExit;

// global data structures
std::deque<std::string> argsDeque;
std::deque<std::string> responseArgsDeque;

void putBufferInArgsDeque(){
    argsDeque.clear();
    std::string buffStr(buff);
    while (buffStr.length()){
        size_t firstSpaceIdx = buffStr.find(" ");
        argsDeque.push_back(buffStr.substr(0, firstSpaceIdx));
        buffStr = buffStr.substr(firstSpaceIdx + 1); //todo check if works-correct
        if (firstSpaceIdx == std::string::npos){ // mo matches
            break;
        }
    }
    // make command argument upper case
    std::transform(argsDeque[0].begin(), argsDeque[0].end(),
                   argsDeque[0].begin(), ::toupper);
}

const std::string getDateFormat()
{ //todo move to aux file
    time_t time;
    char date[80];
    std::time(&time);
    struct tm* tm = localtime(&time);
    strftime(date, 80, "%H:%M:%S", tm);
    return std::string(date);
}

const std::string logDateFormat()
{
    time_t time;
    char date[80];
    std::time(&time);
    struct tm* tm = localtime(&time);
    strftime(date, 80, "%H%M%S", tm);
    return std::string(date);
}

void destruct(){
    int closed = close(clientSocketDesc);
    std::cout << "closed: " << closed << std::endl; // todo remove

    responseArgsDeque.clear();
    argsDeque.clear();
    logFile->close();
    delete(logFile);
}

void writeToStream()
{
    // connect to server
    int connected = connect(clientSocketDesc,
            reinterpret_cast<struct sockaddr *>(&serverAddr),
            sizeof(sockaddr_in));
    std::cout << "connected: " << connected << std::endl;
    if (connected < 0)
    {
        (*logFile) << getDateFormat() << "\tERROR\tconnect\t" <<
                errno << "." << std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }
    std::cout<<"connected successfully"<<std::endl; //todo remove

    // write command to stream
    if (write(clientSocketDesc, msgToServer.c_str(), msgToServer.length()) < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\twrite\t"<<errno<<"."<<std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }
    std::cout<<"parse: write to stream - success"<<std::endl; //todo remove
}

void readFromStream(){
    std::cout<<"parse: read from stream"<<std::endl; //todo remove
    memset(buff, 0, MAX_BUFFER_LENGTH);

    if (read(clientSocketDesc, buff, MAX_BUFFER_LENGTH) < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\tread\t"<<errno<<"."<<std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }
    int closed = close(clientSocketDesc);
    std::cout << "closed socket after read: " << closed << std::endl; // todo remove
}

void parseResponse(){
    std::cout<<"parse: parse response"<<std::endl; //todo remove
    std::string buffStr(buff);

    while (buffStr.length()) {
        size_t dollarIdx = buffStr.find("$");
        responseArgsDeque.push_back(buffStr.substr(0, dollarIdx));
        if (dollarIdx == std::string::npos){
            break;
        }
        buffStr = buffStr.substr(dollarIdx + 1);
    }
}

void parseUserInput(){

    responseArgsDeque.clear();
    msgToServer.append(buff);

    putBufferInArgsDeque();

    msgToServer.append(" ");
    msgToServer.append(clientName);

    std::string command = argsDeque[0];

    if (command == "REGISTER"){
        std::cout<<"parse: register"<<std::endl; //todo remove
        if(argsDeque.size() > 1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                (*logFile)<<getDateFormat()<<"ERROR: invalid argument "<<
                        argsDeque[i]<< "in command REGISTER."<<std::endl;
            }
            return; //todo check
        }
        if (registered){
            destruct();
            exit(EXIT_FAILURE);
        }
        registered = true;

        writeToStream();
        readFromStream();
        parseResponse();
        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

        if (!responseArgsDeque[0].compare("EXIT")){
            // todo should i write to log?
            destruct();
            exit(EXIT_FAILURE);
        }
    }
    else if (command == "CREATE"){
        std::cout<<"parse: create"<<std::endl; //todo remove

        if(argsDeque.size() < 4){
            (*logFile)<<"ERROR: missing arguments in command CREATE."<<std::endl;
            return;
        }
        if (argsDeque.size() > 4){
            for (size_t i = 4; i < argsDeque.size(); i++){
                (*logFile)<<"ERROR: invalid argument "<<
                        argsDeque[i]<<" in command CREATE."<<std::endl;
            }
            return;
        }
        if (!registered){
            (*logFile)<<getDateFormat()<<"ERROR: first command must be:"
                                                 " REGISTER."<<std::endl;
            return;
        }

        writeToStream();
        readFromStream();
        parseResponse();
        (*logFile) <<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

    }
    else if (command == "GET_TOP_5"){
        std::cout<<"parse: get top 5"<<std::endl; //todo remove
        if(argsDeque.size() >  1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                (*logFile)<<"ERROR: invalid argument "<<
                        argsDeque[i]<<" in command GET_TOP_5."<<std::endl;
            }
            return;
        }
        if (!registered) {
            (*logFile) << getDateFormat() << "ERROR: first command must be:"
                    " REGISTER."<<std::endl;
            return;
        }

        writeToStream();
        readFromStream();
        parseResponse();

        // index zero is for GOOD "keyword"
        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;
        for(size_t i = 2; i < responseArgsDeque.size(); i++){
            (*logFile)<<responseArgsDeque[i]<<std::endl;
        }
    }
    else if (command == "SEND_RSVP"){
        std::cout<<"parse: send rsvp"<<std::endl; //todo remove
        if (argsDeque.size() <  2){
            (*logFile)<<getDateFormat()<<"\t"<<
            "ERROR: missing arguments in command SEND_RSVP."<<std::endl;
            return;
        }
        if (argsDeque.size() > 2){
            for (size_t i = 2; i < argsDeque.size(); i++){
                (*logFile)<<getDateFormat()<< "\t"<<
                "ERROR: invalid argument "<<
                        argsDeque[i]<<"in command SEND_RSVP."<<std::endl;
            }
            return;
        }
        if (!registered){
            (*logFile)<<getDateFormat()<<"ERROR: first command must be:"
                                                 " REGISTER."<<std::endl;
            return;
        }


        if (std::transform(argsDeque[1].begin(), argsDeque[1].end(),
                           argsDeque[1].begin(), ::isdigit)
                                                != argsDeque[1].end()){
            (*logFile)<<getDateFormat()<<"\t"<<
            				"ERROR: invalid argument "<<
            				argsDeque[1]<<" in command GET_RSVP_LIST."<<std::endl;
            //todo check spacing
        }
        writeToStream();
        readFromStream();
        parseResponse();

        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<< std::endl;
    }
    else if (command == "GET_RSVPS_LIST"){
        std::cout<<"parse: get rsvps list"<<std::endl; //todo remove
        if(argsDeque.size() <  2){
            (*logFile)<<getDateFormat()<<"\t"<<
            "ERROR: missing arguments in command GET_RSVP_LIST."<<std::endl;
            return;
        }
        if (argsDeque.size() > 2){
            for (size_t i = 2; i < argsDeque.size(); i++){
                (*logFile)<<getDateFormat()<<"\t"<<
                "ERROR: invalid argument "<<
                        argsDeque[i]<< "in command GET_RSVP_LIST."<<std::endl;
            }
            return;
        }

        if (std::transform(argsDeque[1].begin(), argsDeque[1].end(),
                argsDeque[1].begin(), ::isdigit) != argsDeque[1].end()){
            (*logFile)<<getDateFormat()<<"\t"<<
            				"ERROR: invalid argument " <<
            				argsDeque[1]<<" in command GET_RSVP_LIST."<<std::endl;
        }
        if (!registered){
            (*logFile)<<getDateFormat()<<"\t"<<
            "ERROR: first command must be: REGISTER."<<std::endl;
            return;
        }
        writeToStream();
        readFromStream();
        parseResponse();

        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<
                std::endl;


    }
    else if (command == "UNREGISTER"){
        std::cout<<"parse: unregister"<<std::endl; //todo remove
        if(argsDeque.size() >  1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                (*logFile)<<getDateFormat()<<"\t"<<
                "ERROR: invalid argument "<<
                argsDeque[i]<< "in command UNREGISTER."<<std::endl;
            }
            return;
        }
        if (!registered) {
            (*logFile) << getDateFormat() << "\t" <<
            "ERROR: first command must be: REGISTER."<<std::endl;
            return;
        }

        writeToStream();
        readFromStream();
        parseResponse();

        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

        if (!responseArgsDeque[0].compare("GOOD")){ //todo check this
            destruct();
            exit(EXIT_SUCCESS);
        }
        return;

    }
    else{
        std::cout<<"parse: illegal"<<std::endl; //todo remove
        (*logFile)<<getDateFormat()<<"\t"<<"ERROR: illegal command."<<std::endl;
    }

}

int main(int argc , char *argv[])
{
    if(argc != NUMBER_OF_ARGS){ // todo add checks for usage
        std::cout<<"Usage: emClient clientName serverAddress "
                "serverPort"<<std::endl;
        exit(EXIT_FAILURE);
    }

    clientName = argv[1];
    serverIP = gethostbyname(argv[2]);
    port = atoi(argv[3]); // todo vrify in range 0 to 65535

    logFile = new std::ofstream;
//    std::string logName = std::string(clientName).append("_").append( // todo uncomment
//            logDateFormat()).append(".log");

    std::string logName = "emClient.log"; // todo remove

    // open logfile and append
//    (*logFile).open(logName.c_str(), std::ios_base::app); // todo uncomment
    (*logFile).open(logName.c_str()); // todo remove

    if (serverIP == NULL){
        (*logFile)<<getDateFormat()<<"\tERROR\tgethostbyname\t"<<errno<<"."<<std::endl;
        close(clientSocketDesc);
        exit(EXIT_FAILURE);
    }

    memset((char *) &serverAddr, 0, sizeof(sockaddr_in));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((uint16_t) port);
    memcpy((char *)&serverAddr.sin_addr.s_addr, serverIP->h_addr,
           (size_t) serverIP->h_length);

    msgToServer = "";
    registered = false;
    dontExit = true;

    // create socket
    clientSocketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocketDesc < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\tsocket\t"<<errno<<"."<<std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }
    std::cout<<"socket created"<<std::endl; //todo remove

    while (dontExit){
        std::cout << "waiting for input" << std::endl; //todo remove
        msgToServer.clear();
        argsDeque.clear();

        memset(buff, 0, MAX_BUFFER_LENGTH);

        std::cin.get(buff, MAX_BUFFER_LENGTH); // todo remove if not needed
        std::cin.ignore();

//        std::cin >> buff; // todo verify input length / test with large input

        parseUserInput();
    }

    (*logFile).close();

    return 0;
}
