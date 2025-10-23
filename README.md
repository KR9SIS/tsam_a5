# TSAM Assignment 5 - The Botnet Saves the World

## Group Members

- **Vigdís Helga Eyjólfsdóttir** - <vigdis23@ru.is>
- **Friðrik Fannar Söebech** - <fridriks23@ru.is>
- **Kristófer Helgi Sigurðsson** - <kristoferhs22@ru.is>

## Contents

- `tsamgroup9.cpp`
- `client.cpp`
- `Makefile`

## Build

Requires Linux with g++ (C++23). This code was developed using Arch Linux and Ubuntu

```bash
# Builds both at the same time
make

# Build server
make tsamgroup9

# Build client
make client

# Remove tsamgroup9 and client executables
make clean
```

## Executing the Server and Client

Go to the bin directory and then run the following commands

To execute the server

```bash
./tsamgroup9 <port>

# Example
./tsamgroup9 4009
```

To execute the client

```bash
./client <ip_of_server> <port>

# Example
./client 130.208.246.98 4009
```
