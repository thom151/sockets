#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

int main (int argc, char *argv[]) {
    
    int sock_status, bind_status;
    
    int bytes_recv;
    struct addrinfo hints;
    struct addrinfo *res;

    struct sockaddr_storage client_addr;
    socklen_t addr_size;


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


    std::cout<<"listening \n";
    //accept 
    //here, someone will connect() to you and then once you accept() it, you guys
    //will be using a new file descriptor while the old fd will be waiting for other
    //computers that will connect()
     addr_size = sizeof client_addr;
    int new_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_size);
    if (new_fd==-1){
        std::cout<<"error accepting";
        exit(1);
    }

    /*
    std::cout<<"sending hello to client...\n";
    std::string welcome_msg = "Hello from my simple server!\n";
    if (send(new_fd, welcome_msg.c_str(), welcome_msg.length(), 0) == -1) {
        std::cout << "error sending welcome message";
        exit(1);
    }
    */

   
    char buffer[1024];
    std::string full_message="";
    std::string server_msg;

    std::cout<<"starting conversation with client\n\n";

    while (bytes_recv != 0) {
        // a loop to get the full message
        while (1) {
            bytes_recv = recv(new_fd, buffer, sizeof(buffer)-1, 0);
            if (bytes_recv == -1) {
                std::cout<<"error receiving";
                exit(1);
            }
            if (bytes_recv == 0) {
                std::cout<<"client disconnected";
                break;
            } 
            if (buffer[bytes_recv-1] == '\n') {
                full_message.append(buffer, bytes_recv);
                break;
            }     
            full_message.append(buffer, bytes_recv);
        }
        

        std::cout<<"Client: "<<full_message;
        full_message = "";
        if (bytes_recv == 0) {
            std::cout<<"client disconnected";
            break;
        } 


        std::cout<<"Message: ";
        std::getline(std::cin, server_msg);
        server_msg += '\n';
        if (send(new_fd, server_msg.c_str(), server_msg.length(), 0) == -1) {
            perror("server message");
            exit(1);
        }

        std::cout<<"\n";
        
    }

 
    close(new_fd);
    close(fd);

    
    //optional shutdown(sockfd, how);
    //how would be 0: no receives, 1: no send, 2: no both

    //shutdown doesn't free a fd, but close does
}
