//
// Created by asaf on 6/7/16.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <deque>
#include <map>
#include <set>
#include <algorithm>

#define NUMBER_OF_ARGS 2
#define MAX_CONNECTIONS 10
#define MAX_BUFFER_LENGTH 325

typedef struct Event{
public:
    std::string title;
    std::string date;
    std::string description;

    Event(std::string title, std::string date, std::string description){
        this->title = title;
        this->date = date;
        this->description = description;
    }

    bool operator==(struct Event& event){
        return !event.title.compare(this->title) &&
               !event.date.compare(this->date) &&
               !event.description.compare(this->description);

    }
} Event;

// global variables
std::ofstream* logFile;
std::mutex logMutex;
std::mutex dastMutex; //todo write lock and unlock where needed
int nextEventId;
int serverSocketDesc;
std::string msgToClient;
char buff[MAX_BUFFER_LENGTH]; // https://moodle2.cs.huji.ac.il/nu15/mod/forum/discuss.php?d=48066
bool doExit;
// global data structures
std::deque<std::pair<int, std::thread>> threadsDeque;
std::deque<std::string> argsDeque;
std::deque<int> newestEvents;
std::map<int, Event*> eventIdToEvent;
std::map<int, std::set<std::string>*> eventIdRSVPList;
std::map<std::string, std::set<int>*> clientNameToEventId;


const std::string getDateFormat(){
    time_t time;
    char date[80];
    std::time(&time);
    struct tm* tm = localtime(&time);
    strftime(date, 80, "%H:%M:%S", tm);
    return std::string(date);
}


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

void destruct(){
    // close and clear delete resources with logMutex lock
    msgToClient = "EXIT0"; //todo check was is required here- send exit to sockets or users?
    for (auto& kv: threadsDeque){
        if (write(kv.first, msgToClient.c_str(), MAX_BUFFER_LENGTH) < 0){
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tERROR\twrite\t"<<errno<<"."<<std::endl;
            logMutex.unlock();
        }

        close(kv.first);
    }

    close(serverSocketDesc);
    argsDeque.clear();
    newestEvents.clear();
    for (auto& kv: eventIdToEvent){
        delete(kv.second);
    }
    for (auto& kv: eventIdRSVPList){
        kv.second->clear();
        delete(kv.second);
    }
    for (auto& kv: clientNameToEventId){
        kv.second->clear();
        delete(kv.second);
    }
    eventIdToEvent.clear();
    eventIdRSVPList.clear();
    clientNameToEventId.clear();
    logMutex.lock();
    (*logFile).close();
    delete(logFile);
    logMutex.unlock();


}

void waitForExit(){
    std::cout<<"in exit thread"<<std::endl; //todo remove
    std::string buffStr;
    std::deque<std::string> exitArgsDeque;

    while (true){
        // todo implement case when server user input exit before any clients
        // todo connect
        exitArgsDeque.clear();
        buffStr.clear();
        std::cin>>buffStr;

        while(buffStr.length()){
            std::size_t firstSpaceIdx = buffStr.find(" ");
            exitArgsDeque.push_back(buffStr.substr(0, firstSpaceIdx));
            if (firstSpaceIdx == std::string::npos){ // no match
                break;
            }

            buffStr = buffStr.substr(firstSpaceIdx + 1);
        }

        std::transform(exitArgsDeque[0].begin(), exitArgsDeque[0].end(),
                       exitArgsDeque[0].begin(), ::toupper);

        if (exitArgsDeque.size() == 1 && !exitArgsDeque[0].
                compare("EXIT")){ //todo change ! to  == 0. in all places
            exitArgsDeque.clear();
            logMutex.lock();
            (*logFile)<<"EXIT command is typed: server is shutdown"<<std::endl; //todo check about date and \n
            logMutex.unlock();

            doExit = true;
            break;
        }
    }
}

void parseUserInput(){

    putBufferInArgsDeque();

    if (argsDeque.empty()){
        std::cout<<"parse: empty"<<std::endl; //todo remove
        msgToClient.append("GOOD");
        return;
    }

    int eventId;
    std::string clientName;
    std::string command = argsDeque[0];

    if (command == "REGISTER"){
        std::cout<<"parse: register"<<std::endl; //todo remove
        clientName = argsDeque[1];
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            clientNameToEventId[clientName] = new std::set<int>();
            msgToClient.append("GOOD$").append("Client ").append(clientName)
                    .append(" was registered successfully."); //todo is \n needed?
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\t"<<clientName<<
                    "\twas registered successfully."<<std::endl;
            logMutex.unlock();
            
        } else {
            msgToClient.append("EXIT1$").append("Client ").append(clientName)
                    .append(" was already registered.");
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\t"<<"ERROR: "<< clientName <<"\tis "
                                                        "already exists."<<std::endl;
            logMutex.unlock();
        }
    }
    else if (command == "CREATE"){
        std::cout<<"parse: create"<<std::endl; //todo remove
        clientName = argsDeque[4];
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            msgToClient.append("ERROR$").append("ERROR: first command must be: "
                                                        "REGISTER.");
            return;
        }
        Event *event = new Event(argsDeque[1], argsDeque[2], argsDeque[3]);
        for (auto &kv: eventIdToEvent){
            if (*event == *(kv.second)){
                msgToClient.append("ERROR$").append("ERROR: failed to create "
                                     "the event: event already exists.");
                return;
            }
        }

        eventIdToEvent[nextEventId] = event;
        newestEvents.push_front(nextEventId);
        eventIdRSVPList[nextEventId] = new std::set<std::string>();

        msgToClient.append("GOOD$").append("Event id ").append(std::to_string
                      (nextEventId)).append(" was created successfully.$");
        msgToClient.append(std::to_string((nextEventId)));
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<"\tevent id "<<
                nextEventId<<" was assigned to the event with title "<<
                event->title<<"."<<std::endl;
        logMutex.unlock();

        nextEventId++;
    }
    else if (command == "GET_TOP_5"){
        std::cout<<"parse: get top 5"<<std::endl; //todo remove
        clientName = argsDeque[1];
        size_t deque_size = newestEvents.size();

        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command "
                                             "must be: REGISTER.");
            return;
        }

        if (deque_size == 0){
            msgToClient.append("GOOD");
            return;
        } else if (deque_size >= 5) {
            deque_size = 5;
        }

        msgToClient.append("GOOD$").append("Top 5 newest events are:\n");
        for (size_t i = 0; i < deque_size; ++i) {
            (msgToClient.append(std::to_string(newestEvents[i]))).append("\t");
            (msgToClient.append(eventIdToEvent[newestEvents[i]]->title))
                    .append("\t");
            (msgToClient.append(eventIdToEvent[newestEvents[i]]->date))
                    .append("\t");
            (msgToClient.append(eventIdToEvent[newestEvents[i]]->description))
                    .append(".");
            if (i < deque_size - 1) {
                msgToClient.append("$");
            }
        }
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\trequested the top 5 newest events."<<std::endl;
        logMutex.unlock();

    }
    else if (command == "SEND_RSVP"){
        std::cout<<"parse: send rsvp"<<std::endl; //todo remove
        clientName = argsDeque[2];
        eventId = atoi(argsDeque[1].c_str());

        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command must be:"
                                                        " REGISTER.");
            return;
        }

        if (eventIdToEvent.find(eventId) != eventIdToEvent.end()){
            if (eventIdRSVPList[eventId]->find(clientName) !=
                    eventIdRSVPList[eventId]->end()) {
                msgToClient.append("ERROR$").append("RSVP to event id  ")
                        .append(std::to_string(eventId)).append(" was "
                                                          "already sent.");
                return;
            }

            clientNameToEventId[clientName]->insert(eventId);
            eventIdRSVPList[eventId]->insert(clientName);

            msgToClient.append("GOOD$").append("RSVP to event id ")
                    .append(std::to_string(eventId))
                    .append(" was received successfully.");

        } else {
            msgToClient.append("ERROR$").append("ERROR: failed to send RSVP "
                                                        "to event id ")
                    .append(std::to_string(eventId)).append(
                            " Event not found.");
        }
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\tis RSVP to event with id"<<eventId<<"."<<std::endl;
        logMutex.unlock();

    }
    else if (command == "GET_RSVPS_LIST"){
        std::cout<<"parse: get rsvps list"<<std::endl; //todo remove
        clientName = argsDeque[2];
        eventId = atoi(argsDeque[1].c_str());

        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command must "
                                                           "be: REGISTER.");
            return;
        }

        msgToClient.append("GOOD$");
        if (eventIdRSVPList.find(eventId) != eventIdRSVPList.end()){
            msgToClient.append("The RSVP list for event id ")
                    .append(std::to_string(eventId)).append(" is: ");
            size_t i = 0;
            for (auto& new_client_name: *eventIdRSVPList[eventId]) {
                if ( i == eventIdRSVPList[eventId]->size() - 1){
                    msgToClient.append(new_client_name).append(".");
                    break;
                }
                msgToClient.append(new_client_name).append(",");
                i++;
            }
        }
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\trequests the RSVP's list for event with id "<<
        eventId<<"."<<std::endl;
        logMutex.unlock();

    }
    else if (command == "UNREGISTER"){
        std::cout<<"parse: unregister"<<std::endl; //todo remove
        clientName = argsDeque[1];

        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            msgToClient.append("ERROR$").append("ERROR: first command must be: "
                                                        "REGISTER.");
            return;
        }

        for (auto& new_event_tid: *clientNameToEventId[clientName]) {
            eventIdRSVPList[new_event_tid]->erase(clientName);
        }

        clientNameToEventId[clientName]->clear();
        delete (clientNameToEventId[clientName]);
        clientNameToEventId.erase(clientName);

        msgToClient.append("GOOD$").append("Client ")
                .append(clientName)
                .append(" was unregistered successfully.");
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\twas unregistered successfully."<<std::endl;
        logMutex.unlock();

    }
    else {
        std::cout<<"parse: illegal"<<std::endl; //todo remove
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\tERROR\tillegal command."<<std::endl;//todo-remove?
        logMutex.unlock();
    }

}

void readAndWriteToStream(int clientSocketDesc){


    std::cout<<"read from stream"<<std::endl; //todo remove
    //read part


    memset(buff, 0, MAX_BUFFER_LENGTH);
    std::cout<<"read from stream: before read"<<std::endl; //todo remove
    auto bytesRead = read(clientSocketDesc, buff, MAX_BUFFER_LENGTH);
    std::cout<<"read from stream: after read"<<std::endl; //todo remove

    if (bytesRead <= 0) {
        std::cout<<"read from stream: didnt read bytes"<<std::endl;//todo-remove
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tread\t" << errno <<
        "."<<std::endl;
        logMutex.unlock();
    }
    else {
        std::cout<<"read from stream: in else"<<std::endl; //todo remove
        msgToClient.clear();
        parseUserInput();
    }

    //write part
    std::cout<<"write to stream"<<std::endl; //todo remove

    if (write(clientSocketDesc, msgToClient.c_str(), MAX_BUFFER_LENGTH) <
        0) {
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\twrite\t" << errno <<
        "."<<std::endl;
        logMutex.unlock();



    }
}


int main(int argc , char *argv[]) {

    if (argc != NUMBER_OF_ARGS){ // todo add checks for usage
        std::cout<<"Usage: emServer portNum"<<std::endl;
        exit(EXIT_FAILURE);
    }
    nextEventId = 1;
    doExit = false;
    int port = atoi(argv[1]);
    struct sockaddr_in serverAddr, cliAddr;
    socklen_t addressLength = sizeof(sockaddr_in);

    logFile = new std::ofstream;
    (*logFile).open("emServer.log", std::ios_base::app); //append

    memset(&serverAddr, 0, addressLength);

    //Prepare the sockaddr_in structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons((uint16_t) port);

    // create a thread that will listen for the exit cmd on the server keyboard
    std::cout<<"exit thread created"<<std::endl; //todo remove
    std::thread exitThread = std::thread(waitForExit);
    exitThread.detach();



    //Socket
    std::cout<<"socket"<<std::endl; //todo remove
    serverSocketDesc = socket(AF_INET , SOCK_STREAM , 0);
    if(serverSocketDesc < 0){
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\tERROR\tsocket\t"<<errno<<"."<<std::endl;
        logMutex.unlock();
    }

    //Bind
    std::cout<<"bind"<<std::endl; //todo remove
    if( bind(serverSocketDesc, reinterpret_cast<struct sockaddr *>(&serverAddr),
             sizeof(sockaddr_in))
        < 0) {
//    if( bind(serverSocketDesc,(struct sockaddr *)&serverAddr ,
//             sizeof(sockaddr_in))
//        < 0) {
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\tERROR\tbind\t"<<errno<<"."<<std::endl;
        logMutex.unlock();
    }


    //Listen
    std::cout<<"listen"<<std::endl; //todo remove
    listen(serverSocketDesc, MAX_CONNECTIONS);


    while (!doExit){
        //Accept
        std::cout<<"accept"<<std::endl; //todo remove
        int clientSocketDesc = accept(serverSocketDesc, reinterpret_cast<struct sockaddr *>
        (&cliAddr), &addressLength);
//        clientSocketDesc = accept(serverSocketDesc,
//                                  (struct sockaddr *) &cliAddr, &addressLength);
        if (clientSocketDesc < 0){
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tERROR\taccept\t"<<errno<<"."<<std::endl;
            logMutex.unlock();
        }
        //create a thread with the readAndWriteToStream function
        // and push it into the deque
        std::cout<<"create connection thread"<<std::endl; //todo remove
        std::thread readWriteThread(readAndWriteToStream);
        auto threadPair  = std::pair<int, std::thread>(clientSocketDesc, readWriteThread);
        threadsDeque.push_front(threadPair); //todo should we clear finished threads
    }

    //wait for the requests to finish their run
    std::cout<<"join"<<std::endl; //todo remove
    for (auto& kv: threadsDeque){
        kv.second.join();
    }

    destruct();

    return 0;
}
