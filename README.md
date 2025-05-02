# NetRuleGuard : Network Firewall Rule Management System

This repository contains a client-server application designed to manage firewall rules and validate network connections against these rules. The system allows for the addition, deletion, and listing of rules, as well as checking if a specific IP address and port combination is allowed according to the defined rules.

## Components

The repository consists of two main programs:

1. **Server (`server.c`)**: Handles rule management and connection validation
2. **Client (`client.c`)**: Provides a command-line interface to interact with the server

## Compilation

The repository includes a Makefile for easy compilation:

```bash
# Compile both programs
make

# Compile only the server
make server

# Compile only the client
make client

# Clean up compiled files
make clean
```

The Makefile uses the following compilation flags:
- `-Wall`: Enable all compiler warnings
- `-Werror`: Treat warnings as errors
- `-g`: Include debugging information

## Server Usage

The server can be run in two modes:

### Interactive Mode

```bash
./server -i
```

In interactive mode, commands are read from standard input (keyboard), processed, and results are displayed on standard output.

### Network Mode

```bash
./server <port>
```

In network mode, the server listens for commands on the specified port and responds to client requests.

## Client Usage

```bash
./client <serverHost> <serverPort> <command> [<args>...]
```

- `serverHost`: Hostname or IP address of the server
- `serverPort`: Port number the server is listening on
- `command`: Command to send to the server (see Commands section)
- `args`: Additional arguments needed for the command

## Commands

The system supports the following commands:

| Command | Description | Format | Example |
|---------|-------------|--------|---------|
| `A` | Add a rule | `A <ip_range> <port_range>` | `A 192.168.1.0-192.168.1.255 80-443` |
| `D` | Delete a rule | `D <ip_range> <port_range>` | `D 192.168.1.0-192.168.1.255 80-443` |
| `C` | Check connection | `C <ip> <port>` | `C 192.168.1.100 80` |
| `L` | List rules | `L` | `L` |
| `R` | List requests | `R` | `R` |

### IP and Port Range Format

- Single IP: `192.168.1.1`
- IP Range: `192.168.1.0-192.168.1.255`
- Single Port: `80`
- Port Range: `80-443`

## Rule Processing

The system evaluates connection requests against the defined rules. When a connection is checked:

1. The server verifies if the IP and port combination matches any defined rule
2. If a match is found, the connection is accepted
3. Otherwise, the connection is rejected

## Server Response Format

For each command, the server will respond with one of the following messages:

- `Rule added`: Rule was successfully added
- `Invalid rule`: Rule format was incorrect
- `Rule deleted`: Rule was successfully removed
- `Rule not found`: Rule does not exist for deletion
- `Connection accepted`: Connection matches an existing rule
- `Connection rejected`: Connection does not match any rule
- `Illegal IP address or port specified`: Invalid IP or port in request
- `Illegal request`: Invalid command format

## Thread Safety

The server implementation is thread-safe, allowing multiple clients to connect simultaneously in network mode. It uses mutexes to protect the rule and request data structures during concurrent access.

## Memory Management

The system implements proper memory management to avoid memory leaks. The cleanup function ensures all allocated memory is properly freed on program termination.

## Testing

The repository includes several test scripts to validate functionality:

- `test.sh`: Basic functionality tests
- `interactive.sh`: Comprehensive tests for the interactive mode
- `network.sh`: Tests for the network mode

Run the tests with:

```bash
./test.sh
./interactive.sh
./network.sh
```

## Examples

### Adding a rule
```bash
./client localhost 2200 "A 147.188.193.15 22"
```

### Checking a connection
```bash
./client localhost 2200 "C 147.188.193.15 22"
```

### Listing all rules
```bash
./client localhost 2200 "L"
```

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
