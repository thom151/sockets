# SECURITY MONITORING AGENT
A simple linux monitoring agent built using C++ and TCP sockets.

Initially, it started as a project to learn about socket programming and networking in C++. I then expanded it to multi-way communication, then to supporting multiple clients, to designing a protocol, and SSH failed-login monitoring.

## Features
- TCP client and server using C++
- multiple clients
- PING, ECHO, and QUIT commands
- Hostname and uptime reporting
- Failed SSH login monitoring
- SSH events saved to ssh_events.log


## Compile
`g++ -std=c++17 server.cpp -o server
g++ -std=c++17 -pthread client.cpp -o client`

## Run
Start the server:
`./server`

Start the client on a Linux machine:
`./client <server-ip>`

The server listens on port 3490. Try PING, ECHO hello, or QUIT in the client.

To view save SSH events on the server:
`cat ssh_events.log`


## Architecture

The system consists of a client (agent) and a server (collector). The client collects system information and monitors failed SSH login attempts, while the server receives and stores the events.

![System Architecture](architecture.png)


## Command Protocol

A simple protocol was designed to be sent over TCP.


| Command       | Description                                   | 
| ------------- | --------------------------------------------- |
| PING          | Checks communication with the server          |
| ECHO message  | Sends a message for the server to return      |
| QUIT          | Disconnects the client                        |


Example 
`
Client: PING
Server: PONG

Client: ECHO hello
Server: hello

Client: QUIT
Server: BYE!
`

# SSH Monitoring
When an SSH attempt failed, the following message is sent and logged in the server: 
`
--SSH FAILED LOGIN--
Host: linux-machine
Time: 2026-10-27T20:15:00+11:00
User: admin
Source IP: 192.168.1.5
`

## Usage
1. Start the server
`./server`

The server listens for incoming TCP connection on port 3490.

2. Start the client
`./client <server-ip-address>

Replace <server-ip-address> with the IP address of the machine running the server

3. Test communication
Try sending:
`
PING
ECHO hello
QUIT 
`

4. View SSH events
On the server machine:
`cat ssh_events.log`


## What I learned
- `getaddrinfo()` - give an available addr for tcp/udp that someone can connect to given a specific port.
- file descriptors - used to manage connection and got using `socket()`
- `bind()` - can be used to bind a port for a socket to listen to

- Essentially:
    - get addr with deisred specification (i.e., port, tcp, udp, ipv4, ipv6) -> get a file descriptor -> bind to a port -> listen for connection requests -> accept connection -> send/recv data

- A lot of the times I had to make sure that all data will be sent and be received because `recv()` and `send()` doesn't send everthing all at once because of TCP's byte-stream nature.

- I also learned about concurrency and mutexes in C++. Because my client can send messages using the protocol while an ongoing SSH monitoring is happening. Both system uses the same `send_message()` function, and because they use the same fd, I used a mutex to lock and make sure only one system uses the `send_message()` function.

- `pipes()` where it is like a tunnel and we can direct the output to one of the pipes instead of the terminal or `STDOUT_FILENO` using `dup2()`
- `journalctl` is a command in Linux that can be used to get logs of ssh events and other.
- how to handle multiple connection using `poll()`  
- `POLLIN` is an event for accepting clients or receiving from them, `POLLOUT` is for sending.

## 🙏 **Acknowledgments**  
- Inspired [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/split/) 
- Thanks to the C++ community for robust tools and libraries.  


## 📜 **License**  
This project is licensed under the MIT License. Feel free to use, modify, and distribute it as you wish.

