# define compiler and flags
CC=g++
CXXFLAGS=--std=c++11 -Wall -pthread

# executable variables
SERVER_SOURCES=emServer.cpp
CLIENT_SOURCES=emClient.cpp
SERVER_OBJECTS=$(SERVER_SOURCES:.cpp=.o)
CLIENT_OBJECTS=$(CLIENT_SOURCES:.cpp=.o)

# target variables
SERVER_EXEC=emServer
CLIENT_EXEC=emClient
TAR=ex5.tar


.PHONY: all tar clean


all: $(SERVER_EXEC) $(CLIENT_EXEC)

# create the server executable
$(SERVER_EXEC): $(SERVER_SOURCES)
	$(CC) $(CXXFLAGS) $(SERVER_SOURCES) -o $(SERVER_EXEC)

# create the client executable
$(CLIENT_EXEC): $(CLIENT_SOURCES)
	$(CC) $(CXXFLAGS) $(CLIENT_SOURCES) -o $(CLIENT_EXEC)

# create a .tar file for submission
tar: $(SERVER_SOURCES) $(CLIENT_SOURCES) Makefile README
	tar cvf $(TAR) $^

# clean all files created by this Makefile
clean:
	rm -f *.o *.tar $(SERVER_EXEC) $(CLIENT_EXEC)
