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

#define NUMBER_OF_ARGS 4
#define MAX_BUFFER_LENGTH 325

// global variables
std::ofstream logFile;
int port;
struct hostent *serverIP;
std::string clientName;
std::string msgToServer;
char buff[MAX_BUFFER_LENGTH];
int clientSocketDesc;
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





const std::string getDateFormat(){ //todo move to aux file
    time_t time;
    char date[80];
    std::time(&time);
    struct tm* tm = localtime(&time);
    strftime(date, 80, "%H:%M:%S", tm);
    return std::string(date);
}

void writeToStream(){
    if (write(clientSocketDesc, msgToServer.c_str(), msgToServer.length()) < 0){
        logFile<<getDateFormat()<<"\tERROR\twrite\t"<<errno<<".\n";
        exit(EXIT_FAILURE);
    }
}

void readFromStream(){
    memset(buff, 0, MAX_BUFFER_LENGTH);

    if (read(clientSocketDesc, buff, MAX_BUFFER_LENGTH) < 0){
        logFile<<getDateFormat()<<"\tERROR\tread\t"<<errno<<".\n";
        exit(EXIT_FAILURE);
    }

}

void parseResponse(){
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

    msgToServer.append(buff);

    putBufferInArgsDeque();

    msgToServer.append(" ");
    msgToServer.append(clientName);

    std::string command = argsDeque[0];

    if (command == "REGISTER"){
        if(argsDeque.size() > 1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                logFile<<"ERROR: invalid argument "<<
                        argsDeque[i]<< "in command REGISTER.\n";
            }
        }
        writeToStream();
        readFromStream();
        parseResponse();
        logFile<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

        if (!responseArgsDeque[0].compare("EXIT")){
            // todo should i write to log?
            exit(EXIT_FAILURE);
        }
    }
    else if (command == "CREATE"){

        if(argsDeque.size() < 4){
            logFile<<"ERROR: missing arguments in command CREATE.\n";
        }
        if (argsDeque.size() > 4){
            for (size_t i = 4; i < argsDeque.size(); i++){
                logFile<<"ERROR: invalid argument "<<
                        argsDeque[i]<<" in command CREATE.\n";
            }
        }
        writeToStream();
        readFromStream();
        parseResponse();
        logFile <<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

    }
    else if (command == "GET_TOP_5"){

        if(argsDeque.size() >  1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                logFile<<"ERROR: invalid argument "<<
                        argsDeque[i]<<" in command GET_TOP_5.\n";
            }
        }
        writeToStream();
        readFromStream();
        parseResponse();
        for(size_t i = 1; i < responseArgsDeque.size(); i++){
            logFile<<getDateFormat()<<"\t"<<responseArgsDeque[i]<<std::endl;
        }
    }
    else if (command == "SEND_RSVP"){
        if(argsDeque.size() <  2){
            logFile<<getDateFormat()<<"\t"<<
            "ERROR: missing arguments in command SEND_RSVP.\n";
        }
        if (argsDeque.size() > 2){
            for (size_t i = 2; i < argsDeque.size(); i++){
                logFile<<getDateFormat()<< "\t"<<
                "ERROR: invalid argument "<<
                        argsDeque[i]<<"in command SEND_RSVP.\n";
            }
        }

        if (std::transform(argsDeque[1].begin(), argsDeque[1].end(),
                           argsDeque[1].begin(), ::isdigit)
                                                != argsDeque[1].end()){
            //error, change _msg_to_server
            // TODO: error
        }
        writeToStream();
        readFromStream();
        parseResponse();

        logFile<<getDateFormat()<<"\t"<<responseArgsDeque[1]<< std::endl;
    }
    else if (command == "GET_RSVP_LIST"){

        if(argsDeque.size() <  2){
            logFile<<getDateFormat()<<"\t"<<
            "ERROR: missing arguments in command GET_RSVP_LIST.\n";
        }
        if (argsDeque.size() > 2){
            for (size_t i = 2; i < argsDeque.size(); i++){
                logFile<<getDateFormat()<<"\t"<<
                "ERROR: invalid argument "<<
                        argsDeque[i]<< "in command GET_RSVP_LIST.\n";
            }
        }

        if (std::transform(argsDeque[1].begin(), argsDeque[1].end(),
                argsDeque[1].begin(), ::isdigit) != argsDeque[1].end()){
            //error, change _msg_to_server
            // TODO: error
        }
        writeToStream();
        readFromStream();
        parseResponse();

        logFile<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<
                std::endl;


    }
    else if (command == "UNREGISTER"){

        if(argsDeque.size() >  1){
            for (size_t i = 1; i < argsDeque.size(); i++){
                logFile<<getDateFormat()<<"\t"<<
                "ERROR: invalid argument "<<
                argsDeque[i]<< "in command UNREGISTER.\n";
            }
        }

        writeToStream();
        readFromStream();
        parseResponse();

        logFile<<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

        if (responseArgsDeque[0].compare("GOOD")){ //todo check this

            close(clientSocketDesc); //todo ?
            logFile.close(); // todo ?

            exit(0);
        }

    }
    else{
        logFile<<getDateFormat()<<"\t"<<"ERROR: illegal command.\n";
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
    port = atoi(argv[3]);

    if (serverIP == NULL){
        logFile<<getDateFormat()<<"\tERROR\tgethostbyname\t"<<errno<<".\n";
        exit(EXIT_FAILURE);
    }

    logFile.open(std::string(clientName).append("_").append(
            getDateFormat()).append(".log"));

    struct sockaddr_in serverAddr;

    memset((char *) &serverAddr, 0, sizeof(sockaddr_in));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((uint16_t) port);
    memcpy((char *)&serverAddr.sin_addr.s_addr, serverIP->h_addr,
           (size_t) serverIP->h_length);

    msgToServer = "";

    // Socket
    clientSocketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocketDesc < 0){
        logFile<<getDateFormat()<<"\tERROR\tsocket\t"<<errno<<".\n";
        exit(EXIT_FAILURE);
    }

    // Connect
    if (connect(clientSocketDesc, (struct sockaddr *) &serverAddr, sizeof
            (sockaddr_in)) < 0){
        logFile<<getDateFormat()<<"\tERROR\tconnect\t"<<errno<<".\n";
        exit(EXIT_FAILURE);
    }

    while (true){
        memset(buff, 0, MAX_BUFFER_LENGTH);
        std::cin.get(buff, MAX_BUFFER_LENGTH);

        parseUserInput();
    }


    //todo close socket and logfile
    close(clientSocketDesc);
    logFile.close();

    return 0;
}