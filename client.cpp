#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>

#define PORT "3490"

int main(int argc, char *argv[]){
    if (argc != 2) {
        std::cout<<"usage: ./client ip";
        exit(1);
    }
    struct addrinfo hints;
    struct addrinfo *res;
   
    int fd;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;


    int sock_status, bytes_recv;
    

    sock_status = getaddrinfo(argv[1], PORT, &hints, &res);
    if (sock_status != 0 ){
       std::cout << gai_strerror(sock_status) << "\n";
        exit(1);
    }
    
    std::cout << "family: " << res->ai_family << "\n";
    std::cout << "socktype: " << res->ai_socktype << "\n";
    std::cout << "protocol: " << res->ai_protocol << "\n";

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == -1) {
        perror("socket");
        exit(1);
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen)==-1) {
       perror("connect");
        exit(1);
    }

        freeaddrinfo(res);

   
    // this is what we need to fix
    char server_msg[1024];
    std::string full_message;
    std::string clientMessage;
   

    std::cout<<"Starting conversation\n\n";
    while (true) {



        std::cout<<"Message: ";
        std::getline(std::cin, clientMessage);
        clientMessage += '\n';
        if (send(fd, clientMessage.c_str(), clientMessage.length(), 0)== -1) {
            perror("sending client message");
            exit(1);
        }



        while (1) {
            bytes_recv = recv(fd, server_msg, sizeof(server_msg)-1, 0);
            if (bytes_recv == -1) {
                perror("recv");
                exit(1);
            }
            if (bytes_recv == 0) {
                std::cout<<"server disconnected\n";
                break;
            } 

            if (server_msg[bytes_recv-1] == '\n') {
                full_message.append(server_msg, bytes_recv);
                break;
            }
            full_message.append(server_msg, bytes_recv);
        }

        if (bytes_recv == 0) {
                std::cout<<"server disconnected\n";
                break;
        } 



        std::cout<<"Server: "<<full_message;
        full_message="";

        std::cout<<"\n";
    }



    close(fd);
    
}
