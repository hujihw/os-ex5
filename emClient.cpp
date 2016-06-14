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
#define MAX_BUFFER_LENGTH 3250
#define SUCCESS 0;
#define FAILURE -1;
#define MIN_PORT_NUM 1
#define MAX_PORT_NUM 65535

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

void putBufferInArgsDeque()
{
    argsDeque.clear();
    std::string buffStr(buff);
    while (buffStr.length() && argsDeque.size() < 3)
    {
        size_t currSpaceIdx = buffStr.find(" ");
        argsDeque.push_back(buffStr.substr(0, currSpaceIdx));
        buffStr = buffStr.substr(currSpaceIdx + 1);
        if (currSpaceIdx == std::string::npos){ // mo matches
            break;
        }
    }

    if (argsDeque.size() == 3)
    {
        argsDeque.push_back(buffStr);
    }

    // make command argument upper case
    std::transform(argsDeque[0].begin(), argsDeque[0].end(),
                   argsDeque[0].begin(), ::toupper);
}

const std::string getDateFormat()
{
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

void destruct()
{
    responseArgsDeque.clear();
    argsDeque.clear();
    logFile->close();
    delete(logFile);
}

void writeToStream()
{
    // create socket
    clientSocketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocketDesc < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\tsocket\t"<<errno<<"."<<std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }

    // connect to server
    int connected = connect(clientSocketDesc,
            reinterpret_cast<struct sockaddr *>(&serverAddr),
            sizeof(sockaddr_in));
    if (connected < 0)
    {
        (*logFile) << getDateFormat() << "\tERROR\tconnect\t" <<
                errno << "." << std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }

    // write command to stream
    if (write(clientSocketDesc, msgToServer.c_str(), msgToServer.length()) < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\twrite\t"<<errno<<"."<<std::endl;
        destruct();
        exit(EXIT_FAILURE);
    }
}

void readFromStream(){
    memset(buff, 0, MAX_BUFFER_LENGTH);

    if (read(clientSocketDesc, buff, MAX_BUFFER_LENGTH) < 0){
        (*logFile)<<getDateFormat()<<"\tERROR\tread\t"<<errno<<"."<<std::endl;
        int closed = close(clientSocketDesc);
        if (closed < 0)
        {
            (*logFile) << getDateFormat() << "\tERROR\tclose\t"
                << errno << "." << std::endl;
        }
        destruct();
        exit(EXIT_FAILURE);
    }

    int closed = close(clientSocketDesc);
    if (closed < 0)
    {
        (*logFile)<<getDateFormat()<<"\tERROR\tclose\t"<<errno<<"."<<std::endl;
    }
}

void parseResponse()
{
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

/**
 * @brief Verify the number of arguments given
 */
int verifyArgNum(unsigned int num, std::string command)
{
    if (argsDeque.size() != num)
    {
        (*logFile) << getDateFormat()
            << "\tERROR: missing arguments in command " << command
            << "." << std::endl;
        return FAILURE;
    }
    return SUCCESS;
}

int argIsInt(std::string arg, std::string command)
{
    for (auto c: arg)
    {
        if (!(isdigit(c)))
        {
            (*logFile) << getDateFormat()
                << "\tERROR: invalid argument " << arg
                << " in command " << command << "." << std::endl;
            return FAILURE;
        }
    }
    return SUCCESS;
}

void parseUserInput()
{
    responseArgsDeque.clear();
    msgToServer.append(buff);

    putBufferInArgsDeque();

    // append the client name to the end of the command to the server
    msgToServer.append(" ");
    msgToServer.append(clientName);

    std::string command = argsDeque[0];

    if (command == "REGISTER"){
        if (verifyArgNum(1, "REGISTER"))
            return;
        if (!registered)
        {
            writeToStream();
            readFromStream();
            parseResponse();
            (*logFile) << getDateFormat() << "\t"
            << responseArgsDeque[1] << std::endl;

            if (responseArgsDeque[0].compare("EXIT1") == 0)
            {
                destruct();
                exit(EXIT_FAILURE);
            }
            else if (responseArgsDeque[0].compare("ERROR") == 0)
            {

            }
            else
            {
                registered = true;
            }
        }
        else
        {
            (*logFile) << getDateFormat() << "\tERROR: the client "
            << clientName << " was already registered." << std::endl;
        }
    }
    else if (command == "CREATE"){

        if(verifyArgNum(4, "CREATE")){
            return;
        }
        if (!registered){
            (*logFile) << getDateFormat()
                << "\tERROR: first command must be: REGISTER." << std::endl;
            return;
        }

        writeToStream();
        readFromStream();
        parseResponse();
        (*logFile) <<getDateFormat()<<"\t"<<responseArgsDeque[1]<<std::endl;

    }
    else if (command == "GET_TOP_5"){
        if(verifyArgNum(1, "GET_TOP_5"))
        {
            return;
        }
        if (!registered) {
            (*logFile) << getDateFormat()
            << "\tERROR: first command must be: REGISTER." << std::endl;
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
        if(verifyArgNum(2, "SEND_RSVP"))
        {
            return;
        }
        if (!registered){
            (*logFile) << getDateFormat()
            << "\tERROR: first command must be: REGISTER." << std::endl;
            return;
        }
        if (argIsInt(argsDeque[1], "SEND_RSVP"))
        {
            return;
        }
        writeToStream();
        readFromStream();
        parseResponse();

        (*logFile)<<getDateFormat()<<"\t"<<responseArgsDeque[1]<< std::endl;
    }
    else if (command == "GET_RSVPS_LIST"){
        if (verifyArgNum(2, "GET_RSVPS_LIST"))
        {
            return;
        }

        if (std::transform(argsDeque[1].begin(), argsDeque[1].end(),
                argsDeque[1].begin(), ::isdigit) != argsDeque[1].end()){
            (*logFile)<<getDateFormat()<<"\t"<<
            				"ERROR: invalid argument " <<
            				argsDeque[1]<<" in command GET_RSVPS_LIST."<<std::endl;
        }
        if (!registered){
            (*logFile)<<getDateFormat() << "\t" <<
                "ERROR: first command must be: REGISTER."<<std::endl;
            return;
        }
        writeToStream();
        readFromStream();
        parseResponse();

        (*logFile) << getDateFormat()
            << "\t" << responseArgsDeque[1] << std::endl;
    }
    else if (command == "UNREGISTER"){

        if (verifyArgNum(1, "UNREGISTER"))
        {
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

        if (!responseArgsDeque[0].compare("GOOD")){
            destruct();
            exit(EXIT_SUCCESS);
        }
        return;

    }
    else
    {
        (*logFile) << getDateFormat() << "\t"
        << "ERROR: illegal command." << std::endl;
    }
}

int main(int argc , char *argv[])
{
    if(argc != NUMBER_OF_ARGS || MIN_PORT_NUM > atoi(argv[3]) || atoi(argv[3]) > MAX_PORT_NUM){
        std::cout<<"Usage: emClient clientName serverAddress "
                "serverPort"<<std::endl;
        exit(EXIT_FAILURE);
    }

    clientName = argv[1];
    serverIP = gethostbyname(argv[2]);
    port = atoi(argv[3]);



    // open logfile and append
    logFile = new std::ofstream;
    std::string logName = std::string(clientName).append("_").append(
            logDateFormat()).append(".log");
    (*logFile).open(logName.c_str(), std::ios_base::app);


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

    while (dontExit){
        msgToServer.clear();
        argsDeque.clear();

        memset(buff, 0, MAX_BUFFER_LENGTH);

        std::cin.get(buff, MAX_BUFFER_LENGTH);
        std::cin.ignore();

        parseUserInput();

    }

    (*logFile).close();

    return 0;
}