#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ThreadPool.h"

void usage(const char* prog)
{
    std::cout << "Usage: " << prog << " -p <port> [options]\n"
              << "Options:\n"
              << "  -h <host>        target host (default 127.0.0.1)\n"
              << "  -p <port>        target port (required)\n"
              << "  -n <total>       total connections/requests (default 10000)\n"
              << "  -t <threads>     number of threads (default 4)\n";
}

bool send_request(const std::string& host, int port)
{
    std::string request =
        "GET /index.html HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) != 1)
    {
        std::cerr << "inet_pton() failed: " << strerror(errno) << std::endl;
        close(sock);
        return false;
    }
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "connect() failed: " << strerror(errno) << std::endl;
        close(sock);
        return false;
    }

    ssize_t sent = send(sock, request.c_str(), request.size(), 0);
    if (sent <= 0)
    {
        close(sock);
        return false;
    }

    char    buffer[1024];
    ssize_t valread = recv(sock, buffer, sizeof(buffer) - 1, 0);
    bool    ok = false;
    if (valread > 0)
    {
        std::string response(buffer, valread);
        if (response.find("200 OK") != std::string::npos)
        {
            ok = true;
        }
    }

    close(sock);
    return ok;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }

    std::string host = "127.0.0.1";
    int         port = -1;
    int         total = 10000;
    int         threads = 4;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:n:t:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'n':
                total = std::stoi(optarg);
                break;
            case 't':
                threads = std::stoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    if (port <= 0)
    {
        std::cerr << "Error: Port must be specified and greater than 0.\n";
        usage(argv[0]);
        return 1;
    }
    if (threads <= 0)
    {
        std::cerr << "Error: Number of threads must be greater than 0.\n";
        usage(argv[0]);
        return 1;
    }
    if (total <= 0)
    {
        std::cerr << "Error: Total connections/requests must be greater than 0.\n";
        usage(argv[0]);
        return 1;
    }

    // 计算每个线程分配的请求数
    int              per = total / threads;
    int              r = total % threads;
    std::vector<int> counts(threads);
    for (int i = 0; i < threads; ++i)
    {
        counts[i] = per + (i < r ? 1 : 0);
    }

    ThreadPool pool(threads);

    // start barrier: promise + shared future
    // std::promise<void> start_promise;
    // std::shared_future<void> start_fut = start_promise.get_future().share();
    std::condition_variable cv_ready, cv_start;
    std::mutex              cv_m;
    int                     ready = 0;
    bool                    start = false;

    std::vector<std::future<int>> results;
    for (int i = 0; i < threads; ++i)
    {
        int n = counts[i];
        results.emplace_back(pool.enqueue(
            [n, &host, port, &cv_ready, &cv_start, &cv_m, &ready, &start]() -> int
            {
                {
                    std::unique_lock<std::mutex> lock(cv_m);
                    ready++;
                    cv_ready.notify_one();
                    cv_start.wait(lock, [&]() { return start; });
                }
                int success_count = 0;
                for (int j = 0; j < n; ++j)
                {
                    if (send_request(host, port))
                    {
                        ++success_count;
                    }
                }
                return success_count;
            }));
    }

    {
        std::unique_lock<std::mutex> lock(cv_m);
        cv_ready.wait(lock, [&] { return ready == threads; });
        start = true;
    }
    auto t_start = std::chrono::steady_clock::now();
    cv_start.notify_all();

    long total_success = 0;
    for (auto& result : results)
    {
        total_success += result.get();
    }
    auto                          t_end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = t_end - t_start;
    double                        seconds = elapsed.count();
    double qps = seconds > 0.0 ? static_cast<double>(total_success) / seconds : 0.0;

    std::cout << "Total requests: " << total << ", successful: " << total_success
              << ", time: " << std::fixed << std::setprecision(3) << seconds << "s"
              << ", qps: " << std::fixed << std::setprecision(2) << qps << "\n";

    return 0;
}