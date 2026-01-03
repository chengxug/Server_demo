#include <arpa/inet.h>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include "../HttpBuilder.h"
#include "../HttpHandlers.h"
#include "../HttpParser.h"
#include "../HttpRouter.h"
#include "../Socket.h"
#include <stack>

/**
 * 迭代式HTTP服务器实现
 *
 * @details
 * 一个使用使用阻塞式I/O模型迭代处理请求的HTTP服务器，支持短连接，支持静态HTML文件的GET请求处理
 */

/*TODO: 提取为公共函数 */
using handler_t = void (*)(int);
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
              << "  -a <address>     bind address (default 127.0.0.1)\n"
              << "  -p <port>        target port (default 7788)\n"
              << "  -h <help>        display this help message\n";
}

std::shared_ptr<spdlog::logger> g_logger;

void setup_logger(const std::string& name, const std::string& filepath)
{
    g_logger = spdlog::basic_logger_mt(name, filepath);
    g_logger->set_level(spdlog::level::info);   // 设置日志级别为 info
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
}

std::unique_ptr<Router> g_router = register_grouter("WEB_INF");

void handle_client(int client_sock);
int  main(int argc, char* argv[])
{
    setup_logger("iterative_server", "logs/iterative_server.log");
    Signal(SIGINT, sigint_handler);

    std::string address = "127.0.0.1";
    int         port = 7788;

    int opt;
    while ((opt = getopt(argc, argv, "a:p:h")) != -1)
    {
        switch (opt)
        {
            case 'a':
                address = optarg;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    Socket server_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (server_sock.fd() < 0)
    {
        g_logger->error("server_sock creation failed: error code {} errmsg {}",
                        server_sock.getSocketError(),
                        strerror(errno));
        return 1;
    }
    server_sock.setReuseAddr();
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0)
    {
        g_logger->error("Invalid address or inet_pton error: {}", address);
        return 1;
    }
    server_addr.sin_port = htons(port);
    if (!server_sock.bind(server_addr))
    {
        g_logger->error(
            "server_sock bind failed; address: {}, port: {}, error code: {}, errmsg: {}",
            address,
            port,
            server_sock.getSocketError(),
            strerror(errno));
        return 1;
    }
    if (!server_sock.listen(128))
    {
        g_logger->error("server_sock listen failed: error code {}, errmsg {}",
                        server_sock.getSocketError(),
                        strerror(errno));
        return 1;
    }
    std::cout << "Server listening on " << address << ":" << port << std::endl;
    g_logger->info("Server started on {}:{}", address, port);
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t          addr_len = sizeof(client_addr);
        int                client_sock = server_sock.accept(&client_addr, &addr_len);
        if (client_sock < 0)
        {
            g_logger->error("Accept failed: {}", strerror(errno));
            continue;
        }
        handle_client(client_sock);
    }

    return 0;
}

void handle_client(int client_sock)
{
    char buffer[1024];

    HttpReqBuilder req_builder(*g_router, client_sock);
    HttpParser     parser(&req_builder);
    while (true)
    {
        ssize_t nread = ::recv(client_sock, buffer, sizeof(buffer), 0);

        if (nread > 0)
        {
            std::string msg(buffer, static_cast<size_t>(nread));
            g_logger->info("Received message: [{}]", msg);

            parser.feed(msg.data(), msg.size());
            if (req_builder.isDone())
            {
                break;
            }
        }
        else if (nread == 0)   // 对端有序关闭（FIN）
        {
            g_logger->info("Client closed connection (peer performed orderly shutdown)");
            break;
        }
        else   // nread < 0，异常关闭
        {
            // 记录并结束处理clear

            g_logger->warn("recv failed {}", std::strerror(errno));
            break;
        }
    }
    close(client_sock);
}