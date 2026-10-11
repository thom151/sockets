#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <fstream>

#define PORT "3490"

std::string getHostname() {
    char hostname[256];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        exit(1);
    }

    return std::string(hostname);
}

double getUptime() {
    std::ifstream uptime_file("/proc/uptime");

    if (!uptime_file.is_open()) {
        std::cerr << "Failed to open /proc/uptime\n";
        return -1;
    }

    double uptime;
    uptime_file >> uptime;

    return uptime;
}


void readSSHLogs() {
    FILE *pipe = popen("journalctl -t sshd --since \"10 minutes ago\" --no-pager", "r");
    
    if (pipe == nullptr) {
        perror("popen");
        return;
    }

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout<<buffer;
    }

    pclose(pipe);
}

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
    
    std::string hostname_msg = "HOSTNAME " + getHostname() + "\n";
    std::string uptime_msg = "UPTIME " + std::to_string(getUptime()) + "\n";

    if (send(fd, hostname_msg.c_str(), hostname_msg.length(), 0)== -1) {
        perror("sending client message");
        exit(1);
    }
    if (send(fd, uptime_msg.c_str(), uptime_msg.length(), 0)== -1) {
        perror("sending client message");
        exit(1);
    }


    while (true) {

        readSSHLogs();

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
