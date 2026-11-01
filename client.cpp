#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <cstdlib>
#include <thread>
#include <unistd.h>
#include <fstream>
#include <poll.h>
#include <sys/wait.h>

#define PORT "3490"

std::atomic<bool> running{true};
std::mutex send_mutex;


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



bool send_message(int fd, const std::string& message) {
    std::lock_guard<std::mutex> lock(send_mutex);

    size_t total_bytes_sent = 0;
    while(total_bytes_sent < message.size()) {
        ssize_t bytes_sent = send(fd, message.c_str() + total_bytes_sent, message.length() - total_bytes_sent, 0);
        if (bytes_sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            return false;
        }

        if (bytes_sent == 0) {
            return false;
        }
        total_bytes_sent += bytes_sent;
    }

    return true;
}

void monitorSSHLogs(int fd) {
    //pipe[0] read and pipe[1] write
    int pipes[2];
    pipe(pipes);

    pid_t pid = fork();

    if (pid == 0) {
        //in the child process

        close(pipes[0]);
        
        // change writing from terminal to pipes[1]
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);

             execlp(
            "journalctl",
            "journalctl",
            "-t", "sshd",
            "-f",
            "-n", "0",
            "-o", "short-iso",
            "--no-pager",
            nullptr
        );

        _exit(127);
    }

    //in the Parent process
    close(pipes[1]);

    struct pollfd pfd{};
    pfd.fd = pipes[0];
    pfd.events = POLLIN;
    std::string pending;

    char buffer[1024];
    while(running.load()) {
        int result = poll(&pfd, 1, 500);

        if (result > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytes = read(pipes[0],buffer, sizeof(buffer));

            if (bytes <= 0) {
                if (bytes < 0 && errno == EINTR) continue;
                break;
            }

            // something has been read
            pending.append(buffer, bytes);

            size_t pos; 

            while((pos=pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);

                std::string username;
                std::string source_ip;
                std::string timestamp;

                std::istringstream stream(line);
                std::string word;
                if (line.find("Failed password") != std::string::npos) { 
                    stream >> timestamp;
                    while (stream >> word) {
                       if (word == "for") {     
                            stream >> username;
                            if (username == "invalid") {
                                stream >> word;
                                stream >> username;
                            }
                        }

                        if (word == "from") {
                            stream >> source_ip;
                        }

                    }

                    if (!username.empty() && !source_ip.empty() && !timestamp.empty()) {
                           std::string message = "SSH_FAILED " + timestamp + " "
                                + username + " " + source_ip + "\n";
                            //events.push_back(message);
                        if (!send_message(fd, message)) {
                            std::cerr<<"Failed to send SSH event\n";
                        }
                    }
                }

            }
        } else if (pfd.revents & (POLLERR | POLLNVAL)) {
            break;
        }

        if (result < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (result == 0) {
            continue;
        }

    }

    kill(pid, SIGTERM);
    close(pipes[0]);
    waitpid(pid, nullptr, 0);
    
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

    send_message(fd, hostname_msg);
    send_message(fd, uptime_msg);

    std::thread ssh_worker(monitorSSHLogs, fd);
 /*
    for (std::string event :  ssh_events) {
        if (send(fd, event.c_str(), event.length(), 0)== -1) {
            perror("sending client message");
            exit(1);
        }
    }
*/


    while (true) {


        std::cout<<"Message: ";
        std::getline(std::cin, clientMessage);
        clientMessage += '\n';
        if(!send_message(fd, clientMessage)) {
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


    running.store(false);
    ssh_worker.join();
    close(fd);
    
}


void monitorSSHLogs2(int fd) {
    FILE *pipe = popen("journalctl -t sshd -f -n 0 -o short-iso --no-pager","r");
    
    if (pipe == nullptr) {
        perror("popen");
        return;
    }

    char buffer[1024];
    
      while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {

        std::string line(buffer);
        if (line.find("Failed password") != std::string::npos) {
            std::string username;
            std::string source_ip;
            std::string timestamp;

            std::istringstream stream(line);
            std::string word;
           
            stream >> timestamp;
            while (stream >> word) {
               if (word == "for") {     
                    stream >> username;
                    if (username == "invalid") {
                        stream >> word;
                        stream >> username;
                    }
                }

                if (word == "from") {
                    stream >> source_ip;
                }

            }

            if (!username.empty() && !source_ip.empty() && !timestamp.empty()) {
                   std::string message = "SSH_FAILED " + timestamp + " "
                        + username + " " + source_ip + "\n";
                    //events.push_back(message);
                if (!send_message(fd, message)) {
                    std::cerr<<"Failed to send SSH event\n";
                }
            }
        }
    }

    pclose(pipe);
}



