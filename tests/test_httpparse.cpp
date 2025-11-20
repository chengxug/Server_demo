#include "../Http.h"
#include <iostream>
int main()
{
    std::string raw_request = "GET / HTTP/1.1\r\n"
                              "Host: 172.18.195.147:7788\r\n"
                              "Connection: keep-alive\r\n"
                              "Upgrade-Insecure-Requests: 1\r\n"
                              "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/142.0.0.0 Safari/537.36 Edg/142.0.0.0\r\n"
                              "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7\r\n"
                              "Accept-Encoding: gzip, deflate\r\n"
                              "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6\r\n"
                              "\r\n";
    http::HttpParser parser;
    http::HttpRequest request;
    http::ParseResult status = parser.parse(raw_request, request);
    std::cout << "Parse status: " << (status == http::ParseResult::OK ? "OK" : "BAD_REQUEST") << std::endl;
    if (status == http::ParseResult::BAD_REQUEST)
    {
        std::cout << "Error message: " << parser.errorMessage() << std::endl;
    }
    else
    {
        std::cout << "Method: " << request.method << std::endl;
        std::cout << "URI: " << request.uri << std::endl;
        std::cout << "Version: " << request.version << std::endl;
        std::cout << "Headers:" << std::endl;
        for (const auto &header : request.headers)
        {
            std::cout << header.first << ": " << header.second << std::endl;
        }
        std::cout << "Body: " << request.body << std::endl;
    }
    return 0;
}