#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "ThreadPool.h"
class Server
{
public:
    Server(int p) : port_(p), thread_pool_(4), server_fd_(-1) {}
    Server(int p, size_t pool_size) : port_(p), thread_pool_(pool_size), server_fd_(-1) {}
    ~Server() {}

    bool start();
    void stop();

private:
    int port_;
    ThreadPool thread_pool_;
    int server_fd_;
    bool running_;
    std::thread accept_thread;

    bool setup_socket(); // 创建socket, 并设置socket
    void accept_connection();
    void handle_client(int client_sock);
};

// TODO: 添加日志
bool Server::start()
{
    if (!setup_socket())
    {
        std::cerr << "Failed to setup socket" << std::endl;
        return false;
    }

    running_ = true;
    accept_thread = std::thread(&Server::accept_connection, this);
    std::cout << "Server started on port " << port_ << std::endl;

    return true;
}

void Server::stop()
{
    if (running_)
    {
        running_ = false;

        // 关闭Server监听的文件描述符
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
        // 等待 accept_thread 线程退出
        if (accept_thread.joinable())
        {
            accept_thread.join();
        }
        // 关闭日志
    }
}

bool Server::setup_socket()
{
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
    {
        std::cerr << "server_fd_ creation failed" << std::endl;
        return false;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed" << std::endl;
        return false;
    }

    if (listen(server_fd_, 5) < 0)
    {
        std::cerr << "Listen failed" << std::endl;
        return false;
    }

    return true;
}

void Server::accept_connection()
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (running_)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(server_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
        if ((activity < 0) && (errno != EINTR))
        {
            std::cerr << "Select error";
            continue;
        }
        if ((activity > 0) && FD_ISSET(server_fd_, &read_fds))
        {
            int new_sock = accept(server_fd_, (struct sockaddr *)&client_addr, &addr_len);
            if (new_sock < 0)
            {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            thread_pool_.enqueue([this, new_sock]
                                 { handle_client(new_sock); });
        }
    }
}

void Server::handle_client(int client_sock)
{
    char buffer[1024] = {0};
    int nread = read(client_sock, buffer, 1024);
    if (nread > 0)
    {
        std::string msg(buffer);
        std::cout << "Recieved message: [" << msg << "]" << std::endl;

        std::string response(msg);
        send(client_sock, response.c_str(), response.size(), 0);
    }
    close(client_sock);
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    Server server(port);
    if(!server.start())
    {
        return 1;
    }

    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();
    server.stop();
    return 0;
}
