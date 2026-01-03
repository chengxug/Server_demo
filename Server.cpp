#include <dirent.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <stack>
#include <sys/socket.h>
#include <unistd.h>

// #include "Http.h"
#include "HttpBuilder.h"
#include "HttpHandlers.h"
#include "HttpParser.h"
#include "HttpRouter.h"
#include "Logger.h"
#include "Socket.h"
#include "ThreadPool.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

using handler_t = void (*)(int);

class Server
{
public:
    Server(int p)
        : port_(p)
        , thread_pool_(4)
        , server_sock_(-1)
    {
    }
    Server(int p, size_t pool_size)
        : port_(p)
        , thread_pool_(pool_size)
        , server_sock_(-1)
    {
    }
    ~Server() {}

    bool start();
    void stop();

private:
    int                     port_;
    std::unique_ptr<Router> router_;
    ThreadPool              thread_pool_;
    Socket                  server_sock_;
    bool                    running_;
    std::thread             accept_thread;

    bool setup_socket();   // 创建socket, 并设置socket
    void accept_connection();
    void handle_client(int client_sock);
};

bool Server::start()
{
    if (!setup_socket())
    {
        g_logger->error("Failed to setup socket");
        return false;
    }
    router_ = register_grouter("WEB_INF");
    running_ = true;
    accept_thread = std::thread(&Server::accept_connection, this);
    std::cout << "Server started on port " << port_ << std::endl;
    g_logger->info("Server started on port {}", port_);

    return true;
}

void Server::stop()
{
    if (running_)
    {
        running_ = false;

        // 关闭Server监听的文件描述符
        server_sock_.shutdownWrite();
        server_sock_.close();
        // 等待 accept_thread 线程退出
        if (accept_thread.joinable())
        {
            accept_thread.join();
        }
        g_logger->info("Server stopped");
        spdlog::shutdown();
    }
}

bool Server::setup_socket()
{
    server_sock_ = Socket(socket(AF_INET, SOCK_STREAM, 0));
    if (server_sock_.fd() < 0)
    {
        g_logger->error("server_sock_ creation failed: error code {} errmsg {}",
                        server_sock_.getSocketError(),
                        strerror(errno));
        return false;
    }
    server_sock_.setReuseAddr();
    server_sock_.setNonBlocking();
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (!server_sock_.bind(address))
    {
        g_logger->error("server_sock_ bind failed; port: {}, error code: {}, errmsg: {}",
                        port_,
                        server_sock_.getSocketError(),
                        strerror(errno));
        return false;
    }

    if (!server_sock_.listen(128))
    {
        g_logger->error("server_sock_ listen failed: error code {}, errmsg {}",
                        server_sock_.getSocketError(),
                        strerror(errno));
        return false;
    }

    return true;
}

void Server::accept_connection()
{
    struct sockaddr_in client_addr;
    socklen_t          addr_len = sizeof(client_addr);
    while (running_)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_sock_.fd(), &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(server_sock_.fd() + 1, &read_fds, nullptr, nullptr, &timeout);
        if ((activity < 0) && (errno != EINTR))
        {
            g_logger->error("Select error: {}", strerror(errno));
            continue;
        }
        if ((activity > 0) && FD_ISSET(server_sock_.fd(), &read_fds))
        {
            int new_sock = server_sock_.accept(&client_addr, &addr_len);
            if (new_sock < 0)
            {
                g_logger->error("Accept failed: {}", strerror(errno));
                continue;
            }
            thread_pool_.enqueue([this, new_sock] { handle_client(new_sock); });
        }
    }
}

void Server::handle_client(int client_sock)
{
    char buffer[1024] = {0};

    while (true)
    {
        ssize_t nread = ::recv(client_sock, buffer, sizeof(buffer), 0);

        if (nread > 0)
        {
            std::string msg(buffer, static_cast<size_t>(nread));
            g_logger->info("Received message: [{}]", msg);
            HttpReqBuilder req_builder(*router_, client_sock);
            HttpParser     parser(&req_builder);

            parser.feed(msg.data(), msg.size());
            if (req_builder.isDone())
            {
                break;
            }
            continue;
        }
        else if (nread == 0)   // 对端有序关闭（FIN）
        {
            g_logger->info("Client closed connection (peer performed orderly shutdown)");
            break;
        }
        else   // nread < 0，异常关闭
        {
            if (errno == EINTR)   // 被信号中断，重试
            {
                continue;
            }
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK)   // 非阻塞 socket 此时没有数据，继续等待可读事件
            {
                continue;
            }
            // 其他错误，记录并结束处理
            g_logger->warn("recv failed {}", std::strerror(errno));
            break;
        }
    }
    close(client_sock);
}

// 注册信号处理函数的包装函数
handler_t Signal(int signum, handler_t handler)
{
    struct sigaction act, old_act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(signum, &act, &old_act) < 0)
    {
        return SIG_ERR;
    }
    return old_act.sa_handler;
}

void sigint_handler(int signum)
{
    (void)signum;   // unused
    std::cout << "Received SIGINT, shutting down server..." << std::endl;
    g_logger->info("Server stopped");
    spdlog::shutdown();
    exit(0);
}

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -p <port>        target port (default 7788)\n"
              << "  -t <threads>     number of threads (default 4)\n"
              << "  -h               display this help message\n";
}

std::shared_ptr<spdlog::logger> g_logger;

void setup_logger(const std::string& name, const std::string& filepath)
{
    g_logger = spdlog::basic_logger_mt(name, filepath);
    g_logger->set_level(spdlog::level::info);   // 设置日志级别为 info
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

int main(int argc, char* argv[])
{
    setup_logger("Thread_pool_Server_Logger", "logs/thread_pool_server.log");
    Signal(SIGINT, sigint_handler);

    int port = 7788;
    int thread_num = 4;
    int opt;
    while ((opt = getopt(argc, argv, "p:t:h")) != -1)
    {
        switch (opt)
        {
            case 'p':
                port = std::stoi(optarg);
                break;
            case 't':
                thread_num = std::stoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    Server server(port, thread_num);
    if (!server.start())
    {
        std::cout << "Failed to start server" << std::endl;

        return 1;
    }

    std::cout << "Press Ctrl-C to stop the server..." << std::endl;
    while (true)
    {
        pause();   // 等待信号
    }
    return 0;
}