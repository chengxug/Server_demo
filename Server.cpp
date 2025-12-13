#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Http.h"
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
        logger_ = spdlog::basic_logger_mt("basic_logger", "logs/server.log");
    }
    Server(int p, size_t pool_size)
        : port_(p)
        , thread_pool_(pool_size)
        , server_sock_(-1)
    {
        logger_ = spdlog::basic_logger_mt("basic_logger", "logs/server.log");
    }
    ~Server() {}

    bool start();
    void stop();

private:
    int                             port_;
    ThreadPool                      thread_pool_;
    Socket                          server_sock_;
    bool                            running_;
    std::thread                     accept_thread;
    std::shared_ptr<spdlog::logger> logger_;

    bool setup_socket();   // 创建socket, 并设置socket
    void accept_connection();
    void handle_client(int client_sock);
};

bool Server::start()
{
    if (!setup_socket())
    {
        logger_->error("Failed to setup socket");
        return false;
    }

    running_ = true;
    accept_thread = std::thread(&Server::accept_connection, this);
    std::cout << "Server started on port " << port_ << std::endl;
    logger_->info("Server started on port {}", port_);

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
        logger_->info("Server stopped");
        spdlog::shutdown();
    }
}

bool Server::setup_socket()
{
    // server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // if (server_fd_ < 0)
    // {
    //     logger_->error("server_fd_ creation failed");
    //     return false;
    // }
    server_sock_ = Socket(socket(AF_INET, SOCK_STREAM, 0));
    if (server_sock_.fd() < 0)
    {
        logger_->error("server_sock_ creation failed: error code {} errmsg {}",
                       server_sock_.getSocketError(),
                       strerror(errno));
        return false;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (!server_sock_.bind(address))
    {
        logger_->error("server_sock_ bind failed; port: {}, error code: {}, errmsg: {}",
                       port_,
                       server_sock_.getSocketError(),
                       strerror(errno));
        return false;
    }

    if (!server_sock_.listen(128))
    {
        logger_->error("server_sock_ listen failed: error code {}, errmsg {}",
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
            logger_->error("Select error: {}", strerror(errno));
            continue;
        }
        if ((activity > 0) && FD_ISSET(server_sock_.fd(), &read_fds))
        {
            int new_sock = server_sock_.accept(&client_addr, &addr_len);
            if (new_sock < 0)
            {
                logger_->error("Accept failed: {}", strerror(errno));
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
            logger_->info("Received message: [{}]", msg);

            http::HttpParser  parser;
            http::HttpRequest req;
            http::ParseResult status = parser.parse(msg, req);
            if (status != http::ParseResult::OK)
            {
                std::cout << "HTTP Parse Error";
            }
            http::Handler      handler = http::g_router.match(req.method, req.uri);
            http::HttpResponse response;
            if (handler != nullptr)
                handler(req, response);
            else
            {
                std::cout << "ERROR: handler is invalid";
                break;
            }

            std::string result = response.serialize();
            // 确保全部数据被发送（处理部分发送）
            /* send发送的字节数可能小于实际发送的字节数，如以下情况
             *  1. 内核发送缓冲区剩余空间不足；
             *  2. 消息很大，内核分片后多次复制到内核缓冲区；
             *  3. 非阻塞模式，send 会尽可能发送能立即写入的部分然后返回
             */
            const char* data = result.data();
            size_t      remaining = result.size();
            ssize_t     sent_total = 0;
            while (remaining > 0)
            {
                ssize_t sent =
                    send(client_sock,
                         data + sent_total,
                         remaining,
                         MSG_NOSIGNAL);   // MSG_NOSIGNAL 阻止触发 SIGPIPE 信号（否则程序会被终止）
                if (sent < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    logger_->error("send failed: {}", std::strerror(errno));
                    // 发送失败(可能对端已关闭/重置)，退出循环并关闭连接
                    goto cleanup;
                }
                sent_total += sent;
                remaining -= static_cast<size_t>(sent);
            }
            continue;
        }
        else if (nread == 0)   // 对端有序关闭（FIN）
        {
            logger_->info("Client closed connection (peer performed orderly shutdown)");
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
            logger_->warn("recv failed {}", std::strerror(errno));
            break;
        }
    }

cleanup:
    close(client_sock);
}

/*TODO: 异常处理*/
void cat(const http::HttpRequest& request, http::HttpResponse& response)
{
    std::string perfix = "WEB_INF";
    std::string path = "";
    if (request.uri != "/")
        path = perfix + request.uri;
    else
        path = perfix + "/index.html";
    std::ifstream file(path);

    if (!file.is_open())
    {
        response.version = "HTTP/1.1";
        response.status_code = 404;
        response.reason_phrase = "Not Found";
        response.headers["Content-Type"] = "text/html; charset=utf-8";
        response.body = "<html><body><h1>404 Not Found</h1></body></html>";
        return;
    }

    std::string line;
    while (getline(file, line))
    {
        response.body += line + "\n";
    }
    file.close();
    response.version = "HTTP/1.1";
    response.status_code = 200;
    response.reason_phrase = "OK";
    response.headers["Content-Type"] = "text/html; charset=utf-8";
}

void load_path()
{
    http::g_router.addRoute("GET", "/index.html", cat);
    http::g_router.addRoute("GET", "/", cat);
    http::g_router.addRoute("GET", "/Notebook.html", cat);
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

int main(int argc, char* argv[])
{
    load_path();

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

    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();
    server.stop();
    return 0;
}
