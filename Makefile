CXX = g++
FLAGS =  -std=c++23 -Wall -Werror
SRCDIR = src/
BINDIR = bin/
OBJDIR = obj/
INCLUDE = -I ./include/

CLIENTSRC = $(addprefix $(SRCDIR), client.cpp helpers.cpp)
SERVERSRC = $(addprefix $(SRCDIR), tsamgroup9.cpp helpers.cpp)
CLIENTOBJ = $(CLIENTSRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
SERVEROBJ = $(SERVERSRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

CLIENT = $(addprefix $(BINDIR), client)
SERVER = $(addprefix $(BINDIR), tsamgroup9)

PHONY.: client server all

.PHONY: all
all: $(CLIENT) $(SERVER)

.PHONY: client
client: $(CLIENT)

.PHONY: server
server: $(SERVER)

$(CLIENT): $(CLIENTSRC) | $(BINDIR)
	$(CXX) $(CLIENTOBJ) -o $(CLIENT) $(INCLUDE)

$(SERVER): $(SERVERSRC) | $(BINDIR)
	$(CXX) $(SERVEROBJ) -o $(SERVER) $(INCLUDE)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) -c $< -o $@ $(FLAGS) $(INCLUDE)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)
