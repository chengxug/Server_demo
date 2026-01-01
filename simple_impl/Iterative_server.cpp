#include <arpa/inet.h>
#include <dirent.h>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

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

std::unique_ptr<Router>  register_grouter(std::string web_inf_dir);
std::vector<std::string> get_html_files_recursively(const std::string& directory);
void                     handle_client(int client_sock);

std::shared_ptr<spdlog::logger> logger =
    spdlog::basic_logger_mt("basic_logger", "logs/iterative_server.log");
std::unique_ptr<Router> g_router = register_grouter("WEB_INF");

int main(int argc, char* argv[])
{
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
        logger->error("server_sock creation failed: error code {} errmsg {}",
                      server_sock.getSocketError(),
                      strerror(errno));
        return 1;
    }
    server_sock.setReuseAddr();
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0)
    {
        logger->error("Invalid address or inet_pton error: {}", address);
        return 1;
    }
    server_addr.sin_port = htons(port);
    if (!server_sock.bind(server_addr))
    {
        logger->error("server_sock bind failed; address: {}, port: {}, error code: {}, errmsg: {}",
                      address,
                      port,
                      server_sock.getSocketError(),
                      strerror(errno));
        return 1;
    }
    if (!server_sock.listen(128))
    {
        logger->error("server_sock listen failed: error code {}, errmsg {}",
                      server_sock.getSocketError(),
                      strerror(errno));
        return 1;
    }
    std::cout << "Server listening on " << address << ":" << port << std::endl;
    logger->info("Server started on {}:{}", address, port);
    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t          addr_len = sizeof(client_addr);
        int                client_sock = server_sock.accept(&client_addr, &addr_len);
        if (client_sock < 0)
        {
            logger->error("Accept failed: {}", strerror(errno));
            continue;
        }
        handle_client(client_sock);
    }

    return 0;
}

/**
 * HTTP请求构建器和处理器
 *
 * @details 实现HttpParserCallback接口，负责构建HTTP请求对象并调用路由器进行请求处理
 */
class HttpReqBuilder : public HttpParserCallback
{
public:
    HttpReqBuilder(Router& r, int sock)
        : router_(r)
        , client_sock_(sock)
        , done_(false)
    {
    }
    void onRequestLine(const std::string& method, const std::string& path,
                       const std::string& version) override
    {
        (void)version;
        req_.path = path;
        req_.method = (method == "GET") ? HttpMethod::GET : HttpMethod::POST;
    }
    void onHeader(const std::string& name, const std::string& value) override
    {
        req_.headers[name] = value;
    }
    void onHeadersComplete() override
    {
        handler_ = router_.route(req_.method, req_.path, params_);
        if (!handler_)
        {
            send_response(
                client_sock_,
                HttpResponse{
                    404, "Not Found", {{"Content-Length", "0"}, {"Connection", "close"}}, ""});
            logger->info("[404] Not Found: {}", req_.path);
            return;
        }
        handler_->onRequest(req_, params_);
    }
    void onBody(const char* data, size_t len) override
    {
        if (handler_)
            handler_->onBody(data, len);
    }
    void onMessageComplete() override
    {
        if (!handler_)
            return;
        handler_->onEOM();
        HttpResponse resp = handler_->takeResponse();
        resp.headers["Connection"] = "close";
        send_response(client_sock_, resp);
    }
    void onError(int code) override
    {

        send_response(
            client_sock_,
            HttpResponse{
                400, "Bad Request", {{"Content-Length", "0"}, {"Connection", "close"}}, ""});
        logger->error("Parser Error: {}", code);
    }
    bool isDone() const { return done_; }

private:
    HttpRequest                     req_;
    Router&                         router_;
    RouteParams                     params_;
    std::unique_ptr<RequestHandler> handler_;
    int                             client_sock_;
    bool                            done_;
    void                            send_response(int client_sock, const HttpResponse& resp)
    {
        std::string response_str =
            "HTTP/1.1 " + std::to_string(resp.status_code) + " " + resp.status_message + "\r\n";
        for (const auto& header : resp.headers)
        {
            response_str += header.first + ": " + header.second + "\r\n";
        }
        response_str += "\r\n";
        response_str += resp.body;

        ssize_t     total_sent = 0;
        ssize_t     to_send = response_str.size();
        const char* data = response_str.c_str();
        while (total_sent < to_send)
        {
            ssize_t sent = ::send(client_sock, data + total_sent, to_send - total_sent, 0);
            if (sent <= 0)
            {
                logger->error("send failed: {}", std::strerror(errno));
                return;
            }
            total_sent += sent;
        }
    }
};

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
            logger->info("Received message: [{}]", msg);

            parser.feed(msg.data(), msg.size());
            if (req_builder.isDone())
            {
                break;
            }
        }
        else if (nread == 0)   // 对端有序关闭（FIN）
        {
            logger->info("Client closed connection (peer performed orderly shutdown)");
            break;
        }
        else   // nread < 0，异常关闭
        {
            // 记录并结束处理clear

            logger->warn("recv failed {}", std::strerror(errno));
            break;
        }
    }
    close(client_sock);
}

/**
 * 静态 HTML 文件处理器
 * @details 处理对静态 HTML 文件的 GET 请求，读取文件内容并构建 HTTP 响应
 */
class HtmlFileHandler : public RequestHandler
{
public:
    HtmlFileHandler(const std::string& file_path)
        : file_path_(file_path)
    {
    }
    void onRequest(HttpRequest& request, RouteParams& params) override
    {
        (void)params;
        if (request.method != HttpMethod::GET)
        {
            response_.status_code = 405;
            response_.status_message = "Method Not Allowed";
            response_.headers["Content-Length"] = "0";
            logger->info("[405] Method Not Allowed: {}", request.path);
            return;
        }
        std::ifstream file(file_path_);
        if (!file.is_open())
        {
            response_.status_code = 500;
            response_.status_message = "Internal Server Error";
            response_.headers["Content-Length"] = "0";
            logger->error("Failed to open file: {}", file_path_);
            return;
        }
        char buffer[1024];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
        {
            response_.body.append(buffer, file.gcount());
        }
        file.close();
        response_.status_code = 200;
        response_.status_message = "OK";
        response_.headers["Content-Type"] = "text/html";
        response_.headers["Content-Length"] = std::to_string(response_.body.size());
    }
    void onBody(const char* data, size_t len) override
    {
        (void)data;
        (void)len;
    }
    void           onEOM() override {}
    HttpResponse&& takeResponse() override { return std::move(response_); }

private:
    std::string file_path_;
};

/**
 * helper function for register global router
 * @details 扫描指定目录及其子目录，获取所有HTML文件的路径
 *
 * @param directory 目录路径
 *
 * @return HTML文件路径列表
 */
std::vector<std::string> get_html_files_recursively(const std::string& directory)
{
    std::vector<std::string> html_files;
    std::stack<std::string>  dirs;
    dirs.push(directory);

    while (!dirs.empty())
    {
        std::string cur_dir = dirs.top();
        dirs.pop();

        DIR* dir = opendir(cur_dir.c_str());
        if (dir == nullptr)
        {
            logger->error("Failed to open directory: {}", cur_dir);
            continue;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;   // Skip . and ..
            }
            std::string full_path = cur_dir + "/" + entry->d_name;
            if (entry->d_type == DT_DIR)
            {
                dirs.push(full_path);   // Push subdirectory to stack
            }
            else if (entry->d_type == DT_REG)
            {
                if (full_path.size() >= 5 && full_path.substr(full_path.size() - 5) == ".html")
                {
                    html_files.push_back(full_path);
                }
            }
        }
        closedir(dir);
    }
    return html_files;
}

/**
 * 注册全局路由，扫描指定目录下的所有 HTML 文件并注册为静态路由
 * @return 路由对象指针
 */
std::unique_ptr<Router> register_grouter(std::string web_inf_dir)
{
    std::vector<std::string> html_files = get_html_files_recursively(web_inf_dir);
    std::unique_ptr<Router>  router(new Router());
    router->addRoute(HttpMethod::GET,
                     "/",
                     [web_inf_dir]() {
                         return std::unique_ptr<RequestHandler>(
                             new HtmlFileHandler(web_inf_dir + "/index.html"));
                     });
    for (const auto& file_path : html_files)
    {
        router->addRoute(HttpMethod::GET,
                         file_path.substr(web_inf_dir.size()),   // 去掉前缀目录
                         [file_path]() {
                             return std::unique_ptr<RequestHandler>(new HtmlFileHandler(file_path));
                         });
    }
    return router;
}