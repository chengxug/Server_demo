#pragma once
#include "HttpParser.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Router: 基于Radix Tree实现的HTTP请求路由
 */

enum class HttpMethod
{
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS,
    HEAD,
    UNKNOWN
};

struct HttpRequest
{
    HttpMethod  method;
    std::string path;
    Headers     headers;
    std::string body;
};

struct HttpResponse
{
    int         status_code;
    std::string status_message;
    Headers     headers;
    std::string body;
};

using RouteParams = std::unordered_map<std::string, std::string>;

class RequestHandler
{
public:
    virtual void           onRequest(HttpRequest& request, RouteParams& params) = 0;
    virtual void           onBody(const char* data, size_t len) = 0;
    virtual void           onEOM() = 0;
    virtual HttpResponse&& takeResponse() = 0;   // 在请求结束时调用，获取响应
    virtual ~RequestHandler() = default;

protected:
    HttpResponse response_;
};

struct RadixNode
{
    std::string                                          prefix;
    std::unordered_map<char, std::unique_ptr<RadixNode>> static_children;

    std::unique_ptr<RadixNode> param_child;   // 参数节点
    std::string                param_name;    // 参数名称

    std::unique_ptr<RadixNode> wildcard;   // 通配符节点

    std::function<std::unique_ptr<RequestHandler>()> handler_factory;   // 处理器工厂函数
};

class Router
{
public:
    void                            addRoute(HttpMethod method, const std::string& path,
                                             std::function<std::unique_ptr<RequestHandler>()> handler_factory);
    std::unique_ptr<RequestHandler> route(HttpMethod method, const std::string& path,
                                          RouteParams& params);

private:
    std::unordered_map<HttpMethod, std::unique_ptr<RadixNode>> trees_;
    void                            insert(RadixNode* node, const std::string& path,
                                           std::function<std::unique_ptr<RequestHandler>()> handler_factory);
    std::unique_ptr<RequestHandler> match(RadixNode* node, const std::string& path,
                                          RouteParams& params);
};

void Router::addRoute(HttpMethod method, const std::string& path,
                      std::function<std::unique_ptr<RequestHandler>()> handler_factory)
{
    auto& tree = trees_[method];
    if (!tree)
    {
        tree = std::unique_ptr<RadixNode>(new RadixNode());
    }
    insert(tree.get(), path, handler_factory);
}

std::unique_ptr<RequestHandler> Router::route(HttpMethod method, const std::string& path,
                                              RouteParams& params)
{
    auto it = trees_.find(method);
    if (it == trees_.end())
        return nullptr;

    std::string curr_path = path;
    if (!curr_path.empty() && curr_path[0] == '/')
    {
        curr_path = curr_path.substr(1);
    }

    return match(it->second.get(), curr_path, params);
}

void Router::insert(RadixNode* node, const std::string& path,
                    std::function<std::unique_ptr<RequestHandler>()> factory)
{
    std::string curr_path = path;
    if (!curr_path.empty() && curr_path[0] == '/')
    {
        curr_path = curr_path.substr(1);
    }

    if (curr_path.empty())
    {
        node->handler_factory = factory;
        return;
    }

    if (curr_path[0] == ':')
    {
        // param child
        auto        pos = curr_path.find('/');
        std::string param = curr_path.substr(1, pos - 1);
        std::string rest = (pos == std::string::npos) ? "" : curr_path.substr(pos);

        if (!node->param_child)
        {
            node->param_child = std::unique_ptr<RadixNode>(new RadixNode());
            node->param_child->param_name = param;
        }
        insert(node->param_child.get(), rest, factory);
        return;
    }

    if (curr_path[0] == '*')
    {
        // wildcard
        node->wildcard = std::unique_ptr<RadixNode>(new RadixNode());
        node->wildcard->handler_factory = factory;
        return;
    }

    // static child
    auto        pos = curr_path.find('/');
    std::string segment = (pos == std::string::npos) ? curr_path : curr_path.substr(0, pos);
    std::string rest = (pos == std::string::npos) ? "" : curr_path.substr(pos);

    if (!node->static_children.count(segment[0]))
    {
        node->static_children[segment[0]] = std::unique_ptr<RadixNode>(new RadixNode());
        node->static_children[segment[0]]->prefix = segment;
    }

    insert(node->static_children[segment[0]].get(), rest, factory);
}

std::unique_ptr<RequestHandler> Router::match(RadixNode* node, const std::string& path,
                                              RouteParams& params)
{
    if (!node)
        return nullptr;
    if (path.empty())
    {
        if (node->handler_factory)
            return node->handler_factory();
        return nullptr;
    }

    // static child first
    for (auto& item : node->static_children)
    {
        auto& child = item.second;
        if (path.compare(0, child->prefix.size(), child->prefix) == 0)
        {
            auto rest = path.substr(child->prefix.size());
            if (!rest.empty() && rest[0] == '/')
                rest = rest.substr(1);
            auto h = match(child.get(), rest, params);
            if (h)
                return h;
        }
    }

    // param child
    if (node->param_child)
    {
        auto        pos = path.find('/');
        std::string segment = (pos == std::string::npos) ? path : path.substr(0, pos);
        std::string rest = (pos == std::string::npos) ? "" : path.substr(pos + 1);
        params[node->param_child->param_name] = segment;
        auto h = match(node->param_child.get(), rest, params);
        if (h)
            return h;
    }

    // wildcard
    if (node->wildcard)
    {
        params["*"] = path;
        return node->wildcard->handler_factory();
    }

    return nullptr;
}
