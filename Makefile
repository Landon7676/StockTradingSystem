# Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I/usr/include  # Added -I/usr/include
LDFLAGS = -lsqlite3  # Link SQLite3

# Source Files
SRCS = client.cpp server.cpp database.cpp
OBJS = $(SRCS:.cpp=.o)

# Executables
SERVER = server
CLIENT = client

# Default Target
all: $(SERVER) $(CLIENT)

# Compile Server
$(SERVER): server.o database.o
	$(CXX) $(CXXFLAGS) -o $(SERVER) server.o database.o $(LDFLAGS)

# Compile Client
$(CLIENT): client.o
	$(CXX) $(CXXFLAGS) -o $(CLIENT) client.o $(LDFLAGS)

# Compile Source Files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean Build Files
clean:
	rm -f $(OBJS) $(SERVER) $(CLIENT)

# Phony Targets
.PHONY: all clean
