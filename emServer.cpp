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
#define MAX_BUFFER_LENGTH 3250
#define MAX_DATE_LEN 30
#define MIN_PORT_NUM 1
#define MAX_PORT_NUM 65535

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
        return event.title.compare(this->title) == 0 &&
               !event.date.compare(this->date) == 0 &&
               !event.description.compare(this->description) == 0;

    }
} Event;

// global variables
std::ofstream* logFile;
std::mutex logMutex;
std::mutex dastMutex;
int nextEventId;
int serverSocketDesc;
bool doExit;
// global data structures
std::deque<std::thread> threadsDeque;
std::deque<int> newestEvents;
std::map<int, Event*> eventIdToEvent;
std::map<int, std::set<std::string>*> eventIdRSVPList;
std::map<std::string, std::set<int>*> clientNameToEventId;


const std::string getDateFormat(){
    time_t time;
    char date[MAX_DATE_LEN+1];
    std::time(&time);
    struct tm* tm = localtime(&time);
    strftime(date, MAX_DATE_LEN, "%H:%M:%S", tm);
    return std::string(date);
}


void putBufferInArgsDeque(std::deque<std::string>& argsDeque, char * buff){

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
        size_t lastSpaceIdx = buffStr.rfind(" ");
        argsDeque.push_back(buffStr.substr(0, lastSpaceIdx));
        argsDeque.push_back(buffStr.substr(lastSpaceIdx + 1));
    }

    // make command argument upper case
    std::transform(argsDeque[0].begin(), argsDeque[0].end(),
                   argsDeque[0].begin(), ::toupper);
}

void destruct(){

    int ret = close(serverSocketDesc);
    if (ret < 0) {
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tclose\t" << errno << "." << std::endl;
        logMutex.unlock();
    }
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
    std::string buffStr;
    std::deque<std::string> exitArgsDeque;

    while (true){
        exitArgsDeque.clear();
        buffStr.clear();
        std::cin>>buffStr;

        while (buffStr.length()){
            std::size_t firstSpaceIdx = buffStr.find(" ");
            exitArgsDeque.push_back(buffStr.substr(0, firstSpaceIdx));
            if (firstSpaceIdx == std::string::npos){ // no match
                break;
            }

            buffStr = buffStr.substr(firstSpaceIdx + 1);
        }

        std::transform(exitArgsDeque[0].begin(), exitArgsDeque[0].end(),
                       exitArgsDeque[0].begin(), ::toupper);

        if (exitArgsDeque.size() == 1 && exitArgsDeque[0].
                compare("EXIT") == 0){
            exitArgsDeque.clear();
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tEXIT command is typed: server is shutdown."<<std::endl;
            logMutex.unlock();

            doExit = true;
            break;
        }
    }
}

std::string parseUserInput(char * buff){
    std::string msgToClient;

    std::deque<std::string> argsDeque;
    putBufferInArgsDeque(argsDeque, buff);

    if (argsDeque.empty()){
        msgToClient.append("GOOD");
        return msgToClient;
    }

    int eventId;
    std::string clientName;
    std::string command = argsDeque[0];

    if (command == "REGISTER"){
        clientName = argsDeque[1];
        std::unique_lock<std::mutex> dastLock(dastMutex); //locks (unlocks also when scope ends!)
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            clientNameToEventId[clientName] = new std::set<int>();
            msgToClient.append("GOOD$").append("Client ").append(clientName)
                    .append(" was registered successfully.");
            logMutex.lock();
            (*logFile) << getDateFormat() << "\t" << clientName <<
            "\twas registered successfully." << std::endl;
            logMutex.unlock();
        }
        else {
            msgToClient.append("EXIT1$").append("Client ").append(clientName)
                    .append(" was already registered.");
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\t"<<"ERROR: "<< clientName <<"\tis "
                                                        "already exists."<<std::endl;
            logMutex.unlock();
        }
        dastLock.unlock();
    }
    else if (command == "CREATE"){
        clientName = argsDeque[4];
        std::unique_lock<std::mutex> dastLock(dastMutex); //locks (unlocks also when scope ends!)
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            msgToClient.append("ERROR$").append("ERROR: first command must be: "
                                                        "REGISTER.");
            return msgToClient;
        }
        Event *event = new Event(argsDeque[1], argsDeque[2], argsDeque[3]);
        for (auto &kv: eventIdToEvent){
            if (*event == *(kv.second)){
                msgToClient.append("ERROR$").append("ERROR: failed to create "
                                     "the event: event already exists.");
                return msgToClient;
            }
        }

        eventIdToEvent[nextEventId] = event;
        newestEvents.push_front(nextEventId);
        eventIdRSVPList[nextEventId] = new std::set<std::string>();
        dastLock.unlock();

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
        clientName = argsDeque[1];
        std::unique_lock<std::mutex> dastLock1(dastMutex); //locks (unlocks also when scope ends!)
        size_t deque_size = newestEvents.size();

        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command "
                                             "must be: REGISTER.");
            return msgToClient;
        }
        dastLock1.unlock();

        msgToClient.append("GOOD$").append("Top 5 newest events are:\n");
        if (deque_size >= 5) {
             deque_size = 5;
         }

        std::unique_lock<std::mutex> dastLock2(dastMutex); //locks (unlocks also when scope ends!)
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
        dastLock2.unlock();
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\trequests the top 5 newest events."<<std::endl;
        logMutex.unlock();

    }
    else if (command == "SEND_RSVP"){
        clientName = argsDeque[2];
        eventId = atoi(argsDeque[1].c_str());

        std::unique_lock<std::mutex> dastLock(dastMutex); //locks (unlocks also when scope ends!)
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command must be:"
                                                        " REGISTER.");
            return msgToClient;
        }

        if (eventIdToEvent.find(eventId) != eventIdToEvent.end()){
            if (eventIdRSVPList[eventId]->find(clientName) !=
                    eventIdRSVPList[eventId]->end()) {
                msgToClient.append("ERROR$").append("RSVP to event id ")
                        .append(std::to_string(eventId)).append(" was "
                                                          "already sent.");
                logMutex.lock();
                (*logFile)<<getDateFormat()<<"\t"<<clientName<<
                "\tis RSVP to event with id "<<eventId<<"."<<std::endl;
                logMutex.unlock();
                return msgToClient;
            }

            clientNameToEventId[clientName]->insert(eventId);
            eventIdRSVPList[eventId]->insert(clientName);
            dastLock.unlock();
            msgToClient.append("GOOD$").append("RSVP to event id ")
                    .append(std::to_string(eventId))
                    .append(" was received successfully.");

        } else {
            msgToClient.append("ERROR$").append("ERROR: failed to send RSVP "
                                                        "to event id ")
                    .append(std::to_string(eventId)).append(
                            ": event doesn't exists.");
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tERROR\t"<<
            "parseUserInput\tevent doesn't exists."<<std::endl;
            logMutex.unlock();
            return msgToClient;
        }
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\tis RSVP to event with id "<<eventId<<"."<<std::endl;
        logMutex.unlock();

    }
    else if (command == "GET_RSVPS_LIST"){
        clientName = argsDeque[2];
        eventId = atoi(argsDeque[1].c_str());

        std::unique_lock<std::mutex> dastLock(dastMutex); //locks (unlocks also when scope ends!)
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()){
            msgToClient.append("ERROR$").append("ERROR: first command must "
                                                           "be: REGISTER.");
            return msgToClient;
        }

        if (eventIdRSVPList.find(eventId) != eventIdRSVPList.end()){
            msgToClient.append("GOOD$");
            msgToClient.append("The RSVP’s list for event id ")
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
            dastLock.unlock();
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\t"<<clientName<<
            "\trequests the RSVP’s list for event with id "<<
            eventId<<"."<<std::endl;
            logMutex.unlock();
        } else{
            msgToClient.append("ERROR$").append("ERROR\tparseUserInput\tthe event does not exists"
                                                        ".");
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tERROR"
            "\tparseUserInput\tthe event does not exists."<<std::endl;
            logMutex.unlock();
        }


    }
    else if (command == "UNREGISTER"){
        clientName = argsDeque[1];

        std::unique_lock<std::mutex> dastLock(dastMutex); //locks (unlocks also when scope ends!)
        if (clientNameToEventId.find(clientName) == clientNameToEventId.end()) {
            msgToClient.append("ERROR$").append("ERROR: first command must be: "
                                                        "REGISTER.");
            return msgToClient;
        }

        for (auto& new_event_tid: *clientNameToEventId[clientName]) {
            eventIdRSVPList[new_event_tid]->erase(clientName);
        }

        clientNameToEventId[clientName]->clear();
        delete (clientNameToEventId[clientName]);
        clientNameToEventId.erase(clientName);
        dastLock.unlock();

        msgToClient.append("GOOD$").append("Client ")
                .append(clientName)
                .append(" was unregistered successfully.");
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\t"<<clientName<<
        "\twas unregistered successfully."<<std::endl;
        logMutex.unlock();

    }
    else {
        logMutex.lock();
        (*logFile)<<getDateFormat()<<"\tERROR\tillegal command."<<std::endl;
        logMutex.unlock();
    }
    return msgToClient;
}

void readAndWriteToStream(int clientSocketDesc){

    //read part
    char buff[MAX_BUFFER_LENGTH+1];
    std::string msgToClient;

    memset(buff, 0, MAX_BUFFER_LENGTH);

    auto bytesRead = read(clientSocketDesc, buff, MAX_BUFFER_LENGTH);

    if (bytesRead <= 0) {

        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tread\t" << errno <<
        "."<<std::endl;
        logMutex.unlock();
    }
    else {

        msgToClient = parseUserInput(buff);
    }

    //write part

    if (write(clientSocketDesc, msgToClient.c_str(), MAX_BUFFER_LENGTH) <
        0) {
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\twrite\t" << errno <<
        "."<<std::endl;
        logMutex.unlock();

    }
    int ret = close(clientSocketDesc);
    if (ret < 0) {
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tclose\t" << errno << "." << std::endl;
        logMutex.unlock();
    }
}


int main(int argc , char *argv[]) {

    if (argc != NUMBER_OF_ARGS || MIN_PORT_NUM > atoi(argv[1]) || atoi(argv[1]) > MAX_PORT_NUM) {

        std::cout << "Usage: emServer portNum" << std::endl;
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


    //Socket
    serverSocketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocketDesc < 0) {
        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tsocket\t" << errno << "." << std::endl;
        logMutex.unlock();
    }

    //Bind
    if (bind(serverSocketDesc, reinterpret_cast<struct sockaddr *>(&serverAddr),
             sizeof(sockaddr_in))
        < 0) {

        logMutex.lock();
        (*logFile) << getDateFormat() << "\tERROR\tbind\t" << errno << "." << std::endl;
        logMutex.unlock();
    }


    //Listen
    listen(serverSocketDesc, MAX_CONNECTIONS);

    fd_set allClientsFds, readFds;

    FD_ZERO(&allClientsFds);
    FD_ZERO(&readFds);
    FD_SET(serverSocketDesc, &allClientsFds);
    FD_SET(STDIN_FILENO, &allClientsFds);

    while (!doExit){
        readFds = allClientsFds;

        if (select(MAX_CONNECTIONS + 1, &readFds, NULL, NULL, NULL) < 0){
            logMutex.lock();
            (*logFile)<<getDateFormat()<<"\tERROR\tselect\t"<<errno<<"."<<std::endl;
            logMutex.unlock();
        }
        else if (FD_ISSET(serverSocketDesc, &readFds)){
            //Accept
            int clientSocketDesc = accept(serverSocketDesc, reinterpret_cast<struct sockaddr *>
            (&cliAddr), &addressLength);

            if (clientSocketDesc < 0){
                logMutex.lock();
                (*logFile)<<getDateFormat()<<"\tERROR\taccept\t"<<errno<<"."<<std::endl;
                logMutex.unlock();
            }
            //create a thread with the readAndWriteToStream function
            // and push it into the deque
            threadsDeque.push_front(std::thread(readAndWriteToStream, clientSocketDesc));
        }
        else if (FD_ISSET(STDIN_FILENO, &readFds)){
            waitForExit();
        }
    }

    //wait for the requests to finish their run
    for (auto& someThread: threadsDeque){
        someThread.join();
    }

    destruct();

    return 0;
}
