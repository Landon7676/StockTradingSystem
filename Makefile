# Compiler and Flags
CXX = g++
CC = gcc
CXXFLAGS = -Wall -Wextra -std=c++17
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread -ldl  # Link pthread and dl for SQLite3

# Source Files
SRCS = client.cpp server.cpp database.cpp sqlite3.c
OBJS = $(SRCS:.cpp=.o)

# SQLite3 Object File
SQLITE_OBJ = sqlite3.o

# Executables
SERVER = server
CLIENT = client

# Default Target
all: $(SERVER) $(CLIENT)

# Compile SQLite3 separately using GCC
$(SQLITE_OBJ): sqlite3.c
	$(CC) $(CFLAGS) -c sqlite3.c -o $(SQLITE_OBJ) $(LDFLAGS)

# Compile Server
$(SERVER): server.o database.o $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $(SERVER) server.o database.o $(SQLITE_OBJ) $(LDFLAGS)

# Compile Client
$(CLIENT): client.o
	$(CXX) $(CXXFLAGS) -o $(CLIENT) client.o $(LDFLAGS)

# Compile Source Files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Phony Targets
.PHONY: all clean
