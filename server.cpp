#include <cassert>
#include <cctype>
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
    
};


std::vector<std::string> getMessages(std::string data);
std::optional<Command> getCmdFromString(std::string_view string_cmd);
std::optional<Protocol> parseMessage(std::string_view message);



class Client {
    private:
        int m_fd{}, m_bytes_recv;
        char m_buffer[100];
        bool connected;
        std::string m_full_message;
        std::string m_server_msg;
    public:
        Client(int fd)
            : m_fd{fd}, connected{true} {}
    
        void handle() {
            
            while (connected) {
                //a loop to get the full message
                while (true) {
                    m_bytes_recv = recv(m_fd, m_buffer, sizeof(m_buffer)-1, 0);
                    if (m_bytes_recv == -1) {
                        perror("recv");
                        connected = false;
                        break;
                    }

                    if (m_bytes_recv == 0) {
                        break;
                    }

                    if(m_buffer[m_bytes_recv-1] == '\n') {
                        m_full_message.append(m_buffer, m_bytes_recv);
                        break;
                    }
                    m_full_message.append(m_buffer, m_bytes_recv); 
                }
                
                 //client disconnected break out of the loop
                if (m_bytes_recv == 0) {
                    std::cout<<"client disconnected";
                    break;
                }

                if(!connected) {
                    break;
                }

            // handle the full message of client 
            std::vector<std::string> messages= getMessages(m_full_message);
            m_server_msg ="";
            for (std::string message : messages) { 
                std::cout<<"Client: "<<message<<"\n";

                std::optional<Protocol> protocol = parseMessage(message);

                if (!protocol.has_value()) {
                    m_server_msg += "Invalid Command\n";
                } else {
                    switch (protocol->cmd) {
                        case Command::PING:
                            m_server_msg += "PONG\n";
                            break;
                        case Command::ECHO:
                            m_server_msg += protocol->value;
                            m_server_msg += "\n";
                            break;
                        case Command::QUIT:
                            m_server_msg += "quitting\n";
                            connected = false;
                            break;
                    }
                }
            }

           m_full_message = "";
           if (send(m_fd, m_server_msg.c_str(), m_server_msg.length(), 0) == -1) {
                perror("server message");
                exit(1);
            }

            std::cout<<"\n";
            
        }

    }

    ~Client() {
        close(m_fd);
    }
};


int main (int argc, char *argv[]) {


    
    int sock_status, bind_status;
     
    int bytes_recv;
    struct addrinfo hints;
    struct addrinfo *res;

    struct sockaddr_storage client_addr;
    socklen_t addr_size;

    struct pollfd pfds[FD_MAX];

    std::vector<ClientData> clients;
    int clients_count;

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

                    ClientData client{new_fd, ""};
                    clients.emplace_back(client);
                    clients_count++;

                } else {
                    //recv from client
                    int bytes_recv = recv(pfds[i].fd, buffer, sizeof(buffer)-1,0);

                      if (bytes_recv == -1) {
                        perror("recv");
                        break;
                    }

                    if (bytes_recv == 0) {
                        //TODO: client disconnected so, remove them from pfds
                        break;
                    }
                    
                    clients[i-1].full_message.append(buffer, bytes_recv);
                    if (buffer[bytes_recv-1] == '\n') {
                        std::string server_msg ="";
                        std::vector<std::string> messages = getMessages(clients[i-1].full_message);
                        for(std::string message: messages) {
                            std::optional<Protocol> protocol = parseMessage(message);
                            if (!protocol.has_value()) {
                                server_msg += "Invalid Command\n";
                            } else {
                                switch (protocol->cmd) {
                                    case Command::PING:
                                        server_msg += "PONG\n";
                                        break;
                                    case Command::ECHO:
                                        server_msg += protocol->value;
                                        server_msg += "\n";
                                        break;
                                    case Command::QUIT:
                                        server_msg += "quitting\n";
                                        break;                                
                                }
                                
                            }
                        }
                        clients[i-1].full_message = "";
                        if (send(pfds[i].fd, server_msg.c_str(), server_msg.length(), 0) == -1) {
                            perror("server send()");
                            exit(1);
                        }

                    }



                    //send data
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


std::vector<std::string> getMessages(std::string data) {
    std::vector<std::string> messages;
    int start_index = 0;
    for (int i=0; i<data.length(); i++) {
        if (data[i] == '\n') {
            messages.push_back(data.substr(start_index, i-start_index));
            start_index = i+1;
        }
    }
    return messages;
}





std::optional<Command> getCmdFromString(std::string_view string_cmd) {
    if (string_cmd == "PING") return Command::PING;
    if (string_cmd == "ECHO") return Command::ECHO;
    if (string_cmd == "QUIT") return Command::QUIT;

    return {};
}


std::optional<Protocol> parseMessage(std::string_view message) {
    assert(message != "");
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





