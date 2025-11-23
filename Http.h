#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>
#include <functional>
#include <optional>
#include <unordered_map>

/* HTTP 协议解析和路由的实现
 * 路由使用 Radix tree实现
 */
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

        std::string serialize()
        {
            std::ostringstream response_stream;

            // 状态行: HTTP/1.1 200 OK
            response_stream << version << " "
                            << status_code << " "
                            << reason_phrase << "\r\n";

            // 处理 headers
            // 如果 body 不为空，自动设置 Content-Length（如果未手动设置）
            if (!body.empty() && headers.find("Content-Length") == headers.end())
            {
                headers["Content-Length"] = std::to_string(body.length());
            }

            // 写入所有 headers
            for (const auto &header : headers)
            {
                response_stream << header.first << ": " << header.second << "\r\n";
            }

            // 空行分隔头部和正文
            response_stream << "\r\n";

            // 响应体
            response_stream << body;

            return response_stream.str();
        };
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

    class RadixRouteNode
    {
    public:
        std::string label;
        std::string method;
        bool isEnd;
        Handler handler;
        std::unordered_map<char, RadixRouteNode *> children;
        RadixRouteNode(const std::string &lbl = "") : label(lbl), isEnd(false) {}
        ~RadixRouteNode()
        {
            for (auto pair : children)
            {
                delete pair.second;
            }
        }
    };

    class RadixRouter
    {
    public:
        RadixRouter(std::string rt = "/") : root(new RadixRouteNode(rt)) {}
        void addRoute(const std::string &method, const std::string &uri, Handler handler);
        Handler match(const std::string &method, const std::string &uri) const;
        void printRouter(); // 调试用
    private:
        RadixRouteNode *root;
        void splitNode(RadixRouteNode *node, int splitPos);
        void printHelper(RadixRouteNode *node, const std::string &perfix, bool isLast); // 调试用
    };

    void RadixRouter::splitNode(RadixRouteNode *node, int splitPos)
    {
        std::string reamainLable = node->label.substr(splitPos);
        RadixRouteNode *newNode = new RadixRouteNode(reamainLable);
        newNode->children = std::move(node->children);
        newNode->handler = node->handler;
        newNode->method = node->method;
        newNode->isEnd = node->isEnd;

        node->label = node->label.substr(0, splitPos);
        node->children[reamainLable[0]] = newNode;
        node->handler = nullptr;
        node->method = "";
        node->isEnd = false;
    }

    void RadixRouter::printHelper(RadixRouteNode *node, const std::string &prefix, bool isLast)
    {
        std::string connector = isLast ? "└── " : "├── ";
        std::cout << prefix << connector;

        if (node == root)
        {
            std::cout << "/";
        }
        else
        {
            std::cout << "\"" << node->label << "\"";
            if (node->isEnd)
                std::cout << " [END]";
        }
        std::cout << std::endl;

        std::string newPrefix = prefix + (isLast ? "    " : "│   ");
        int count = 0;
        for (auto &child : node->children)
        {
            bool lastChild = (++count == node->children.size());
            printHelper(child.second, newPrefix, lastChild);
        }
    }

    void RadixRouter::addRoute(const std::string &method, const std::string &uri, Handler handler)
    {
        if (method.empty() || uri.empty() || handler == nullptr)
        {
            std::cout << "Add invalid route";
            return;
        }

        RadixRouteNode *current = root;
        int pos = 0;

        // 深度遍历，插入节点
        while (pos < uri.length())
        {
            char nextChar = uri[pos];
            if (current->children.find(nextChar) == current->children.end()) // pos位置的字符不存在，从pos位置开始创建子节点
            {
                current->children[nextChar] = new RadixRouteNode(uri.substr(pos));
                current->children[nextChar]->isEnd = true;
                current->children[nextChar]->handler = handler;
                current->children[nextChar]->method = method;
                return;
            }

            RadixRouteNode *child = current->children[nextChar];
            std::string label = child->label;

            // 查找公共前缀
            int i = 0;
            while (i < label.length() && pos + i < uri.length() && label[i] == uri[pos + i])
            {
                i++;
            }

            if (i < label.length()) // 公共前缀不覆盖子节点，需要分裂
            {
                splitNode(child, i);
                if (pos + i == uri.length())
                {
                    child->isEnd = true;
                }
                else
                {
                    RadixRouteNode *newNode = new RadixRouteNode(uri.substr(pos + i));
                    newNode->isEnd = true;
                    newNode->handler = handler;
                    newNode->method = method;
                    child->children[uri[pos + i]] = newNode;
                }
                return;
            }

            pos += i;
            current = child;
        }

        std::cout << "why here set current-isEnd, uri:[" << uri << "]";
        // current->isEnd = true;
    }

    Handler RadixRouter::match(const std::string &method, const std::string &uri) const
    {
        RadixRouteNode *current = root;
        int pos = 0;

        while (pos < uri.length())
        {
            if (current->children.find(uri[pos]) == current->children.end())
            {
                return nullptr;
            }

            RadixRouteNode *child = current->children[uri[pos]];
            std::string label = child->label;
            if (uri.length() - pos < label.length())
            {
                return nullptr;
            }
            for (int i = 0; i < label.length(); i++)
            {
                if (uri[pos + i] != label[i])
                {
                    return nullptr;
                }
            }
            pos += label.length();
            current = child;
        }
        return current->handler;
    }

    void RadixRouter::printRouter()
    {
        std::cout << "Router: " << std::endl;
        printHelper(root, "", true);
    }

    RadixRouter g_router; // global router
}