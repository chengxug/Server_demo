#include "../HttpParser.h"
#include "../HttpRouter.h"
#include <cassert>
#include <iostream>
#include <memory>

// 1. 定义一个用于测试的 Handler，用于验证路由是否正确调用
class TestHandler : public RequestHandler
{
public:
    std::string name_;

    TestHandler(const std::string& name)
        : name_(name)
    {
    }

    void onRequest(HttpRequest& request, RouteParams& params) override
    {
        std::cout << "[Success] Matched Handler: " << name_ << std::endl;
        if (!params.empty())
        {
            std::cout << "  Params:" << std::endl;
            for (const auto& p : params)
            {
                std::cout << "    " << p.first << " = " << p.second << std::endl;
            }
        }
    }

    void onBody(const char* data, size_t len) override {}
    void onEOM() override {}
};

// 2. 修复 HttpRequestBuilder (虽然单元测试主要直接测 Router，但保留这个类用于集成测试)
class HttpRequestBuilder : public HttpParserCallback   // 修改：添加 public 继承
{
public:   // 修改：构造函数需要 public
    HttpRequest                     req_;
    Router&                         router_;
    std::unique_ptr<RequestHandler> handler_;
    RouteParams                     params_;

    HttpRequestBuilder(Router& r)
        : router_(r)
    {
    }

    void onRequestLine(const std::string& method, const std::string& path,
                       const std::string& version) override
    {
        req_.path = path;
        req_.method = (method == "GET") ? HttpMethod::GET : HttpMethod::POST;
    }

    // 修改：确保签名与基类一致 (假设基类是 const std::string&)
    void onHeader(const std::string& name, const std::string& value) override
    {
        req_.headers[name] = value;
    }

    void onHeadersComplete() override
    {
        handler_ = router_.route(req_.method, req_.path, params_);
        if (!handler_)
        {
            std::cout << "[404] Not Found: " << req_.path << "\n";
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
        if (handler_)
            handler_->onEOM();   // 修改：注意大小写，基类通常是 onEOM 或 onEom
    }

    void onError(int code) override { std::cout << "Parser Error: " << code << "\n"; }
};

// 辅助函数：注册路由
void add_test_route(Router& router, HttpMethod method, const std::string& path,
                    const std::string& handler_name)
{
    router.addRoute(method,
                    path,
                    [handler_name]()
                    { return std::unique_ptr<RequestHandler>(new TestHandler(handler_name)); });
}

// 辅助函数：执行测试
void test_route(Router& router, HttpMethod method, const std::string& path)
{
    std::cout << "Testing Route: " << path << " ... ";
    RouteParams params;
    auto        handler = router.route(method, path, params);

    if (handler)
    {
        HttpRequest req;   // 模拟请求对象
        req.method = method;
        req.path = path;
        handler->onRequest(req, params);
    }
    else
    {
        std::cout << "[Failed] No handler found (404)" << std::endl;
    }
    std::cout << "----------------------------------------" << std::endl;
}

int main()
{
    Router router;

    // --- 1. 注册路由 ---
    std::cout << "=== Registering Routes ===" << std::endl;

    // 静态路由
    add_test_route(router, HttpMethod::GET, "/api/version", "static_version");
    add_test_route(router, HttpMethod::POST, "/api/login", "static_login");

    // 参数路由
    add_test_route(router, HttpMethod::GET, "/users/:id", "param_user_id");
    add_test_route(router, HttpMethod::GET, "/users/:id/profile", "param_user_profile");

    // 通配符路由
    add_test_route(router, HttpMethod::GET, "/static/*", "wildcard_static");

    std::cout << std::endl;

    // --- 2. 执行测试用例 ---
    std::cout << "=== Running Tests ===" << std::endl;

    // Case 1: 静态路由匹配
    test_route(router, HttpMethod::GET, "/api/version");

    // Case 2: 参数路由匹配
    test_route(router, HttpMethod::GET, "/users/1001");

    // Case 3: 嵌套参数路由匹配
    test_route(router, HttpMethod::GET, "/users/1001/profile");

    // Case 4: 通配符路由匹配
    test_route(router, HttpMethod::GET, "/static/css/style.css");
    test_route(router, HttpMethod::GET, "/static/js/app.js");

    // Case 5: 方法不匹配 (POST 路由用 GET 访问)
    std::cout << "Testing Method Mismatch (Expect 404): ";
    test_route(router, HttpMethod::GET, "/api/login");

    // Case 6: 路径不存在
    std::cout << "Testing Non-existent Path (Expect 404): ";
    test_route(router, HttpMethod::GET, "/api/unknown");

    return 0;
}