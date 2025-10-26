# TSAM Assignment 5 - The Botnet Saves the World

## Group Members

- **Vigdís Helga Eyjólfsdóttir** - <vigdis23@ru.is>
- **Friðrik Fannar Söebech** - <fridriks23@ru.is>
- **Kristófer Helgi Sigurðsson** - <kristoferhs22@ru.is>

## Contents

### Main files

- `tsamgroup9.cpp`
- `client.cpp`
- `Makefile`

### Additional files

**src files:**

- `block_bots.cpp`
- `client_logs.cpp`
- `helpers.cpp`
- `operations.cpp`

**include (header) files:**

- `block_bots.hpp`
- `client_logs.hpp`
- `helpers.h`
- `operations.hpp`
- `serv_client.hpp`
- `server.hpp`

## Build

Requires Linux with g++ (C++23). This code was developed using Arch Linux and Ubuntu

```bash
# Builds both at the same time
make

# Builds both at the same time and copies tsamgroup9 to the tsam server (IP 130.208.246.98)
make && scp ./bin/tsamgroup9 <user_name>@130.208.246.98:~/<path_to_directory_from_user_home_dir>

# Example:
make && scp ./bin/tsamgroup9 vigdis23@130.208.246.98:~/A5/

# Build server
make tsamgroup9

# Build client
make client

# Remove tsamgroup9 and client executables
make clean
```

## Executing the Server and Client

Go to the bin directory and then run the following commands:

To execute the server:

```bash
./tsamgroup9 <port>

# Example
./tsamgroup9 4009
```

To execute the client:

```bash
./client <ip_of_server> <port>

# Example
./client 130.208.246.98 4009
```

## Server

The server works autonimously, and so it does not receive any input from the
terminal, in addition to be able to connect to a client where the client can
send commands to the server or send messages through the server. The server
does not need to be connected to a client to work. When starting the server
starts by trying to connect to the instructor servers (Instr_1 and Instr_2),
and send them the HELO command to get them to send a list of servers they are
connected to. The server then goes through the list and tries to connect to
all the servers in the list.

The server can maintain a connection to at most 8 other servers, where it
prioratises connections to other groups (named A5_`<num>`), so if it is at full
capacity and trying to connect to another server, if connected to an instructor
it closes that connection, if not it closes the oldest connection.

The server keeps timestamped logs of all commands sent and received, with other
information about the server additionally. The logs are both printed in the
terminal and written to a log file, which it creates when starting. The log file
can then be found in the same directory as the executable file (tsamgroup9).

The server sends messages and commands using the format:
`<SOH><length><STX><command><ETX>` as stated in the project description.
When receiving a message or a command, the server uses the length to extract the
command instead of the tags, to make sure it does not accidentially truncate a
part of the command.

### Commands implemented in the server

**HELO:**

```bash
HELO,<FROM_GROUP_ID>

# Example
HELO,A5_9
```

The server automatically sends this commands to all servers it is connected to.
Additionally, it automatically sends this command to newly connected servers.
The server also accepts this command and replies with the SERVERS command bellow.

**SERVERS:**

```bash
SERVERS,<Name_of_current_server>,<IP_of_current_server>,<Port_of_current_server>;<Server_name>,<Server_IP>,<Server_port>;

# Example
SERVERS,A5_9,130.208.246.98,4009;Instr_1,130.208.246.98,5001;
```

This is a reply to a received HELO command, where it lists the directly connected
servers to this servers. It starts with itself, and lists all the directly
connected servers after that, specifying their name, the host ip and the port
they accept connections on. It is comma seperated within the message, and each
server is seperated by ";". When receiving the SERVERS command from another server,
it tries to connect to all the servers in that list that it is not currently
connected to, where if it has reached the max capacity of connections it prioratises
other groups over instructors, and if it is not connected to any instructors
then it closes its oldest connection.

**KEEPALIVE:**

```bash
KEEPALIVE,<num_of_messages>

# Example
KEEPALIVE,2
```

The server periodically sends this command to its directly connected servers to
keep the connection open, specifying the number of messages the server has waiting
for the receipient. If the server receives the KEEPALIVE it checks the number of
messages the other server has for it, and sends the GETMSGS command to the other
server if they are more than 0, to get the messages it has for it.

**GETMSGS:**

```bash
GETMSGS,<GROUP_ID>

# Example
GETMSGS,A5_9
```

If the server gets this command, it sends the messages it is storing for the
group specified to the client who sent this command.
If the server receives a KEEPALIVE command from another server, with the number
of messages being more than 0, then the server sends this command to that server
to get the messages the other server is storing for it.

**SENDMSGS:**

```bash
SENDMSG,<TO_GROUP_ID>,<FROM_GROUP_ID>,<message_content>

# If the servers group ID is neither the to group nor the from group
SENDMSG,<TO_GROUP_ID>,<FROM_GROUP_ID>,<message_content>,<SERVERS_GROUP_ID>

# Example
SENDMSG,A5_1,A5_9,Hello there
SENDMSG,A5_1,A5_2,Hello there,A5_9
```

Sends a message to another group, with the whole package not exceeding 5000 bytes.
If the message is not from the server nor to it, then it adds it's group ID to
the end, and sends it along.

**STATUSREQ:**

```bash
STATUSREQ
```

The server periodically sends this command to other servers.
If the server receives the command, it responds with the STATUSRESP command bellow.

**STATUSRESP:**

```bash
STATUSRESP,<server_ID, messages_held>

# Example
STATUSRESP,Instr_1,3,A5_1,2
```

If the server receives the STATUSREQ command from another server,
then it responds with the STATUSRESP command, which is a comma seperated list of
server names and the number of messages it has for them.

## Client

The client only connects to one server through the IP and port number stated in the
commandline when executing the client. It is only possible to send messages to other
servers through the client, as it can accept input from the terminal.
The client prints out a list of available commands and sends the selected command
to the server. The client can communicate to the server using selected commands,
seen bellow.
Additionally, the client prints time stamped logs to the terminal and to a log file.
Logs include all commands sent and responses received, and some useful information.
When the client is executed, it creates a log file and writes the logs to it,
where it can be found in the logs folder under the name "client_logs.txt".

### Commands implemented in the client

**GETMSG:**

```bash
GETMSG
```

Gets a message from the server for the client's group.

**SENDMSG:**

```bash
SENDMSG,<TO_GROUP_ID>,<message_contents>

# Example
SENDMSG,Inst_1,Hello there
```

Let's the server deliver the message to the selected group ID.

**LISTSERVERS:**

```bash
LISTSERVERS
```

Lists the servers the clients server is connected to. The client sends the HELO
command to the server, where the server responds with the SERVERS command which
lists the servers it is connected to.

**STATUSREQ:**

```bash
STATUSREQ

# Example response
STATUSRESP,Instr_1,3,A5_1,2
```

Gets the status of the server it is connected to. The server responds with the
STATUSRESP command, whith a comma seperated list of server names and number of
messages the server has for them.

## Assignment Logs

### c) Accept connection from another server

```text
[Sat Oct 25 17:36:33 2025] [tmp] [INFO] Accepting Client [IP]: 130.208.246.98 [ID]: tmp [PORT]: 11472
[Sat Oct 25 17:36:33 2025] [tmp] [RESPONSE] tmp sent HELO,A5_21
[Sat Oct 25 17:36:33 2025] [A5_21] [RECV_HELO] A5_21
[Sat Oct 25 17:36:33 2025] [A5_21] [INFO] Updated client name: tmp to A5_21
```

### d) Recieve Messages from other servers

```text
[Sat Oct 25 17:16:44 2025] [A5_1] [RESPONSE] A5_1 sent SENDMSG,A5_9,A5_1,Hallo fancy manA5_1
```

### d) + e) + Indirect Bot + chatty bott

**Group A5_42 - Chatty bot and Indirect bot:**
```text
[Sat Oct 25 00:32:56 2025] [A5_42] [RESPONSE] A5_42 sent SENDMSG,Instr_2,A5_42,Hello thereA5_16,A5_153,A5_152,A5_151
[Sat Oct 25 00:32:56 2025] [Instr_2] [SENT] SENDMSG,Instr_2,A5_42,Hello thereA5_16,A5_153,A5_152,A5_151
```

```text
[Sat Oct 25 00:32:56 2025] [A5_42] [RESPONSE] A5_42 sent SENDMSG,Instr_2,A5_8,I don't know how to parse messages correctlyA5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_152,A5_151
[Sat Oct 25 00:32:56 2025] [Instr_2] [SENT] SENDMSG,Instr_2,A5_8,I don't know how to parse messages correctlyA5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_16,A5_153,A5_152,A5_151
```

**Group A5_60 - Chatty bot:**
```text
[Sun Oct 26 03:41:55 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_60,A5_9,Gimme another automated message plz :3
[Sun Oct 26 03:41:55 2025] [Instr_1] [SENT] SENDMSG,A5_60,A5_9,Gimme another automated message plz :3
[Sun Oct 26 03:43:10 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_60,A5_9,plz
[Sun Oct 26 03:43:10 2025] [Instr_1] [SENT] SENDMSG,A5_60,A5_9,plz
[Sun Oct 26 03:44:58 2025] [A5_60] [RESPONSE] A5_60 sent SENDMSG,A5_9,A5_60,Hello from A5_60 automated!
```

**Group A5_1 - Chatty bot:**
```text
[Fri Oct 24 10:25:32 2025] [A5_1] [RESPONSE] A5_1 sent SENDMSG,A5_9,A5_1,hæA5_1
[Fri Oct 24 10:27:30 2025] [A5_1] [RESPONSE] A5_1 sent SENDMSG,A5_9,A5_1,sorry for the loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong messageA5_1
```

**Group A5_52 - Chatty bot:**
```text
[Sun Oct 26 17:28:11 2025] [A5_52] [RESPONSE] A5_52 sent SENDMSG,A5_9,A5_52,Hello from group 52!
[Sun Oct 26 17:30:09 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_52,A5_9,Hilloooo group 52 :3 zanku for msg!!
[Sun Oct 26 17:30:09 2025] [A5_52] [SENT] SENDMSG,A5_52,A5_9,Hilloooo group 52 :3 zanku for msg!!

[Sun Oct 26 17:52:59 2025] [A5_52] [RESPONSE] A5_52 sent SENDMSG,A5_9,Instr_2,What do you do if a message contains some weird characters and another message, such as FSENDMSG,Instr_2,A5_9,I don't know how to parse messages correctly?

[Sun Oct 26 17:53:25 2025] [A5_52] [RESPONSE] A5_52 sent SENDMSG,A5_9,Instr_2,Sorry about this really looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong message!

[Sun Oct 26 17:58:10 2025] [A5_52] [RESPONSE] A5_52 sent SENDMSG,A5_9,Instr_2,What do you do if a message contains some weird characters and another message, such as FSENDMSG,Instr_2,A5_9,I don't know how to parse messages correctly?

[Sun Oct 26 17:08:35 2025] [A5_52] [RESPONSE] A5_52 sent SENDMSG,A5_9,A5_52,some message
```

**Group 16 - Chatty bot and Indirect bot:**
```text
[Sun Oct 26 17:35:48 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_16,A5_9,Heloooo A5_16!! hope youre having a lovely day :3
[Sun Oct 26 17:35:48 2025] [A5_16] [SENT] SENDMSG,A5_16,A5_9,Heloooo A5_16!! hope youre having a lovely day :3
[Sun Oct 26 17:39:09 2025] [A5_16] [RESPONSE] A5_16 sent SENDMSG,,A5_16,A5_16
[Sun Oct 26 17:39:09 2025] [Instr_1] [SENT] SENDMSG,,A5_16,A5_16,A5_9
[Sun Oct 26 17:39:39 2025] [A5_16] [RESPONSE] A5_16 sent SENDMSG,A5_70,A5_16,hellohelloA5_16
```

**Group 70 - Chatty bot and Indirect bot:**
```text
[Sun Oct 26 17:39:39 2025] [A5_16] [RESPONSE] A5_16 sent SENDMSG,A5_70,A5_16,hellohelloA5_16
[Sun Oct 26 17:39:39 2025] [Instr_1] [SENT] SENDMSG,A5_70,A5_16,hellohelloA5_16,A5_9
```

**Group A5_24 - Chatty bot:**
```text
[Fri Oct 24 10:24:50 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_24,A5_9,Hellooooo
```

**Group A5_35 - Chatty bot and Indirect bot:**
```text
[Sun Oct 26 16:29:53 2025] [A5_35] [RESPONSE] A5_35 sent SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:29:53 2025] [A5_35] [SENT] SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:29:53 2025] [A5_35] [RESPONSE] A5_35 sent SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:29:53 2025] [A5_35] [SENT] SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:31:55 2025] [A5_35] [RESPONSE] A5_35 sent SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:31:55 2025] [A5_35] [SENT] SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:31:55 2025] [A5_35] [RESPONSE] A5_35 sent SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:31:55 2025] [A5_35] [SENT] SENDMSG,A5_35,NUMBER,
[Sun Oct 26 16:37:39 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_35,A5_9,I have received some of the relayed messages :D :3
[Sun Oct 26 16:37:39 2025] [Instr_1] [SENT] SENDMSG,A5_35,A5_9,I have received some of the relayed messages :D :3A5_9
```

**Group A5_92 - Chatty bot:**
```text
[Sun Oct 26 16:46:03 2025] [A5_9] [RESPONSE] A5_9 sent SENDMSG,A5_92,A5_9,Guess we both use quite a bit of the cpu, trying to fix ':D
[Sun Oct 26 16:47:17 2025] [A5_92] [SENT] SENDMSG,A5_92,A5_9,Guess we both use quite a bit of the cpu, trying to fix ':D
```

**Group A5_21 And A5_32 - Chatty bot and Indirect bot:**
```text
[Sun Oct 26 17:00:01 2025] [A5_21] [RESPONSE] A5_21 sent SENDMSG,A5_32,Instr_2,I guess you know that messages need to end with ETX () and start with SOH ().
[Sun Oct 26 17:00:01 2025] [A5_52] [SENT] SENDMSG,A5_32,Instr_2,I guess you know that messages need to end with ETX () and start with SOH ().A5_9
```

**Group A5_29 - Chatty bot:**
```text
[Sun Oct 26 17:03:02 2025] [A5_29] [RESPONSE] A5_29 sent SENDMSG,A5_9,Instr_2,I guess you know that messages need to end with ETX () and start with SOH ().A5_29
[Sun Oct 26 17:03:02 2025] [A5_29] [RESPONSE] A5_29 sent SENDMSG,A5_9,Instr_2,What do you do if a message contains some weird characters and another message, such as FSENDMSG,Instr_2,A5_9,I don't know how to parse messages correctly?A5_29
```

### Edgy bot

Note: Logs got wiped that showed more

```text
[Sun Oct 26 16:39:47 2025] [A5_29] [SENT] SERVERS,A5_9,130.208.246.98,4009;A5_60,130.208.246.98,4060;A5_9,31.209.201.240,41748;A5_70,130.208.243.61,4070;A5_29,46.182.188.192,52716;

[Sun Oct 26 17:41:17 2025] [A5_92] [SENT] SERVERS,A5_9,130.208.246.98,4009;A5_52,130.208.246.98,65535;A5_9,31.209.201.240,48280;A5_16,130.208.246.98,4016;A5_21,130.208.246.98,4021;A5_92,130.208.246.98,40248;

[Fri Oct 24 03:47:01 2025] [A5_70] [SENT] SERVERS,A5_9,130.208.246.98,4009;A5_52,130.208.246.98,65535;A5_60,130.208.246.98,4060;A5_70,130.208.246.98,38838;
```

**Total:**
- c) Chatty bot - 12 groups
- d) Indirect bot - 5
- e) Edgy bot - 3
