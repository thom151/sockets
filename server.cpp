#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <poll.h>

#define FD_MAX 10

enum class Command {
    PING,
    ECHO,
    QUIT,
   
};

struct Protocol {
    Command cmd;
    std::string value;
};

struct ClientData {
    int fd;
    std::string full_message;
    std::string server_msg;
    bool disconnect_after_send;
    
};


std::optional<Command> getCmdFromString(std::string_view string_cmd);
std::optional<Protocol> parseMessage(std::string_view message);





int main (int argc, char *argv[]) {


    
    int sock_status, bind_status;
     
    int bytes_recv;
    struct addrinfo hints;
    struct addrinfo *res;

    struct sockaddr_storage client_addr;
    socklen_t addr_size;

    struct pollfd pfds[FD_MAX];

    std::vector<ClientData> clients;

    memset(&hints, 0, sizeof(hints));

    //this basically means give me any ipv4/6(af_unspec) tcp(sock_stream) address(ai_passive) that someone can connect to
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    sock_status = getaddrinfo(NULL, "3490", &hints, &res);
    if (sock_status != 0) {
        std::cout << "error";
        exit(1);
    }


    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        std::cout << "error creating socket\n";
        exit(1);
    }

    //bind a port for a socket to listen to
    bind_status = bind(fd, res->ai_addr, res->ai_addrlen);
    if (bind_status != 0) {
        std::cout <<"error binding";
            exit(1);
    }

    freeaddrinfo(res);


    //listen
    if (listen(fd, 10) == -1) {
        std::cout<<"error listening";
        exit(1);
    }

   
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;
    int fd_count = 1;
    std::cout<<"listening \n";
    //accept 
    //here, someone will connect() to you and then once you accept() it, you guys
    //will be using a new file descriptor while the old fd will be waiting for other
    //computers that will connect()
    
    char buffer[256]; 

    while (true) {

        //call poll()
        int num_events = poll(pfds, fd_count, 2500);
    
        if (num_events == -1) {
            perror("poll");
            exit(1);
        }

        if (num_events == 0) {
                printf("Poll timed out\n");
        } 

        //check if any events happening including acccepting clients
        for (int i=0; i< fd_count; ++i) {
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == fd) {
                    //accept clients
                    addr_size = sizeof client_addr;
                    int new_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_size);
                    if (new_fd==-1){
                        std::cout<<"error accepting";
                        exit(1);
                    }

                    pfds[fd_count].fd = new_fd;
                    pfds[fd_count].events = POLLIN;
                    fd_count++;

                    ClientData client{new_fd, "", "", false};
                    clients.emplace_back(client);

                } else {
                    //recv from client
                    int bytes_recv = recv(pfds[i].fd, buffer, sizeof(buffer)-1,0);

                      if (bytes_recv == -1) {
                        perror("recv");
                        break;
                    }

                    if (bytes_recv == 0) {
                           close(pfds[i].fd);

                        pfds[i] = pfds[fd_count - 1];
                        clients[i - 1] = clients[fd_count - 2];

                        clients.pop_back();
                        fd_count--;

                        i--;
                        continue;
                    }
                    
                    clients[i-1].full_message.append(buffer, bytes_recv);

                    size_t newline_pos = clients[i-1].full_message.find('\n');
                    while (newline_pos != std::string::npos) {
                        std::string message = clients[i-1].full_message.substr(0, newline_pos);
                        clients[i-1].full_message.erase(0, newline_pos+1);
                        
                        std::optional<Protocol> protocol = parseMessage(message);
                        if (!protocol.has_value()) {
                            clients[i-1].server_msg += "Invalid Command\n";
                            pfds[i].events |= POLLOUT;
                        } else {
                            switch (protocol->cmd) {
                                case Command::PING:
                                    clients[i-1].server_msg += "PONG\n";
                                    pfds[i].events |= POLLOUT;
                                    break;
                                case Command::ECHO:
                                    clients[i-1].server_msg += protocol->value;
                                    clients[i-1].server_msg += "\n";
                                    pfds[i].events |= POLLOUT;
                                    break;
                                case Command::QUIT:
                                    clients[i-1].server_msg += "BYE!\n";
                                    clients[i-1].disconnect_after_send = true;
                                    pfds[i].events |= POLLOUT;
                                    break;                                
                            }
                            
                        }

                        if (clients[i-1].disconnect_after_send) {
                            break;
                        }
                        newline_pos = clients[i-1].full_message.find('\n');

                    } 

                }
            }

            if (pfds[i].revents & POLLOUT) {
                int bytes_sent = send(pfds[i].fd, clients[i-1].server_msg.c_str(), clients[i-1].server_msg.length(), 0);
                if (bytes_sent== -1) {
                    perror("server send()");
                    exit(1);
                }

               if (bytes_sent < clients[i-1].server_msg.length()) {
                    //update the server msg to the remaining bytes
                    clients[i-1].server_msg = clients[i-1].server_msg.substr(bytes_sent);
                } else if (bytes_sent == clients[i-1].server_msg.length()) {
                    clients[i-1].server_msg = "";
                    pfds[i].events &= ~POLLOUT;


                    if (clients[i-1].disconnect_after_send) {
                        close(pfds[i].fd);

                        //remove pfds[i] (swapping with last)
                        pfds[i] = pfds[fd_count-1];

                        //remove clients[i-1]; (swapping with last)
                        clients[i-1] = clients[fd_count-2];
                        clients.pop_back();
                        fd_count--;

                        //checl the swapped fd
                        i--; 
                        continue;

                    }
                }
            }

        }
        
    } 
    
    //clean up function
    close(fd);

    
    //optional shutdown(sockfd, how);
    //how would be 0: no receives, 1: no send, 2: no both

    //shutdown doesn't free a fd, but close does
}




std::optional<Command> getCmdFromString(std::string_view string_cmd) {
    if (string_cmd == "PING") return Command::PING;
    if (string_cmd == "ECHO") return Command::ECHO;
    if (string_cmd == "QUIT") return Command::QUIT;

    return {};
}


std::optional<Protocol> parseMessage(std::string_view message) {
    size_t space_index = message.find(' ');
    std::string_view command_str;
    std::string value;

    if (space_index == std::string::npos) {
        command_str = message;
        value = "";
    } else {
        command_str = message.substr(0, space_index);
        value = message.substr(space_index + 1);
    }



    std::optional<Command> cmd = getCmdFromString(command_str);
    if (!cmd.has_value()) {
        return {};
    }


    Protocol protocol;
    switch (cmd.value()) {
        case Command::PING:
            if (!value.empty()) return {};
            protocol.cmd = cmd.value();
            return protocol;

        case Command::ECHO:
            protocol.cmd = cmd.value();
            protocol.value = value;
            return protocol;

        case Command::QUIT:
            if (!value.empty()) return {};
            protocol.cmd = cmd.value();
            return protocol;

        default:
            return {};

    }


}





