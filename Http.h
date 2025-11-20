#pragma once
#include <unordered_map>
#include <string>
#include <functional>
#include <optional>

namespace http
{
    constexpr const char *CRLF = "\r\n";
    using Headers = std::unordered_map<std::string, std::string>;

    struct HttpRequest
    {
        std::string method;
        std::string uri;
        std::string version;
        Headers headers;
        std::string body;
    };

    struct HttpResponse
    {
        std::string version;
        int status_code;
        std::string reason_phrase;
        Headers headers;
        std::string body;
    };

    enum class ParseResult
    {
        OK,
        BAD_REQUEST
    };

    class HttpParser
    {
    public:
        ParseResult parse(const std::string &data, HttpRequest &request)
        {
            size_t pos = 0;
            size_t line_end = data.find(CRLF, pos);
            if (line_end == std::string::npos)
            {
                err_msg = "INCOMPLETE REQUEST LINE, missing CRLF";
                return ParseResult::BAD_REQUEST;
            }
            std::string request_line = data.substr(pos, line_end - pos);
            pos = line_end + 2;
            size_t method_end = request_line.find(' ');
            if (method_end == std::string::npos)
            {
                err_msg = "INVALID REQUEST LINE, missing spaces";
                return ParseResult::BAD_REQUEST;
            }
            request.method = request_line.substr(0, method_end);
            size_t uri_end = request_line.find(' ', method_end + 1);
            if (uri_end == std::string::npos)
            {
                err_msg = "INVALID REQUEST LINE, missing version";
                return ParseResult::BAD_REQUEST;
            }
            request.uri = request_line.substr(method_end + 1, uri_end - method_end - 1);
            request.version = request_line.substr(uri_end + 1);

            while (true)
            {
                line_end = data.find(CRLF, pos);
                if (line_end == std::string::npos)
                {
                    err_msg = "INCOMPLETE HEADER LINE, missing CRLF";
                    return ParseResult::BAD_REQUEST;
                }
                if (line_end == pos) // 空行，头部结束
                {
                    pos += 2;
                    break;
                }
                std::string header_line = data.substr(pos, line_end - pos);
                size_t colon_pos = header_line.find(':');
                if (colon_pos == std::string::npos)
                {
                    err_msg = "INVALID HEADER LINE, missing colon";
                    return ParseResult::BAD_REQUEST;
                }
                std::string header_name = header_line.substr(0, colon_pos);
                std::string header_value = header_line.substr(colon_pos + 1);
                // 去除header_value前后的空白字符
                size_t value_start = header_value.find_first_not_of(" \t");
                size_t value_end = header_value.find_last_not_of(" \t");
                if (value_start != std::string::npos && value_end != std::string::npos)
                {
                    header_value = header_value.substr(value_start, value_end - value_start + 1);
                }
                request.headers[header_name] = header_value;
                pos = line_end + 2;
            }

            request.body = data.substr(pos);
            return ParseResult::OK;
        }
        std::string errorMessage() const
        {
            return err_msg;
        }

    private:
        std::string err_msg;
    };

    using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;
    class Router
    {
    public:
        void addRoute(const std::string &method, const std::string &uri, Handler handler);
        Handler match(const std::string &method, const std::string &uri) const;
    };
}