#include "../HttpRouter.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

/**
 * MockHandler: 用于验证 Router 是否正确创建了预期的 Handler
 */
class MockHandler : public RequestHandler
{
public:
    std::string id_;

    explicit MockHandler(std::string id)
        : id_(std::move(id))
    {
    }

    void           onRequest(HttpRequest&, RouteParams&) override {}
    void           onBody(const char*, size_t) override {}
    void           onEOM() override {}
    HttpResponse&& takeResponse() override { return std::move(response_); }
};

// 辅助函数：创建返回特定 ID MockHandler 的工厂
std::function<std::unique_ptr<RequestHandler>()> make_factory(const std::string& id)
{
    return [id]() -> std::unique_ptr<RequestHandler>
    { return std::unique_ptr<RequestHandler>(new MockHandler(id)); };
}

// 辅助函数：用于断言路由匹配结果
void assert_match(Router& router, HttpMethod method, const std::string& path,
                  const char* expected_id)
{
    RouteParams params;
    auto        handler = router.route(method, path, params);
    if (expected_id == nullptr)
    {
        if (handler != nullptr)
        {
            std::cerr << "Test Failed: Expected no match for " << path << ", but got match."
                      << std::endl;
            std::exit(1);
        }
    }
    else
    {
        if (handler == nullptr)
        {
            std::cerr << "Test Failed: Expected match for " << path << ", but got nullptr."
                      << std::endl;
            std::exit(1);
        }
        auto* mock = dynamic_cast<MockHandler*>(handler.get());
        if (mock->id_ != expected_id)
        {
            std::cerr << "Test Failed: Expected handler '" << expected_id << "' for " << path
                      << ", but got '" << mock->id_ << "'." << std::endl;
            std::exit(1);
        }
    }
}

// 辅助函数，用于断言参数提取结果
void assert_param(Router& router, HttpMethod method, const std::string& path,
                  const std::string& param_key, const std::string& param_value)
{
    RouteParams params;
    auto        handler = router.route(method, path, params);
    if (handler == nullptr)
    {
        std::cerr << "Test Failed: Expected match for " << path << ", but got nullptr."
                  << std::endl;
        std::exit(1);
    }
    if (params.find(param_key) == params.end() || params[param_key] != param_value)
    {
        std::cerr << "Test Failed: Expected param " << param_key << "=" << param_value << " for "
                  << path << std::endl;
        std::exit(1);
    }
}

int main()
{
    try
    {
        // Test 1: 精确路径匹配 (Exact Path Matching)
        // 验证 Router 能够正确匹配完全相等的静态路径
        {
            std::cout << "Test 1: Exact Path Matching..." << std::endl;
            Router router;
            router.addRoute(HttpMethod::GET, "/hello", make_factory("hello_handler"));
            router.addRoute(HttpMethod::GET, "/user/profile", make_factory("profile_handler"));

            assert_match(router, HttpMethod::GET, "/hello", "hello_handler");
            assert_match(router, HttpMethod::GET, "/user/profile", "profile_handler");

            // 边界：前导斜杠处理
            assert_match(router, HttpMethod::GET, "hello", "hello_handler");
        }

        // Test 2: HTTP 方法区分 (Method Differentiation)
        // 验证相同的路径在不同的 HTTP Method 下路由到不同的 Handler
        {
            std::cout << "Test 2: Method Differentiation..." << std::endl;
            Router router;
            router.addRoute(HttpMethod::GET, "/api/item", make_factory("get_item"));
            router.addRoute(HttpMethod::POST, "/api/item", make_factory("create_item"));
            router.addRoute(HttpMethod::DELETE, "/api/item", make_factory("delete_item"));

            assert_match(router, HttpMethod::GET, "/api/item", "get_item");
            assert_match(router, HttpMethod::POST, "/api/item", "create_item");
            assert_match(router, HttpMethod::DELETE, "/api/item", "delete_item");

            // 未注册的方法应返回空
            assert_match(router, HttpMethod::PUT, "/api/item", nullptr);
        }

        // Test 3: 路径参数提取 (Path Parameter Extraction)
        // 验证 Router 能够识别 :param 语法并正确提取参数值
        {
            std::cout << "Test 3: Path Parameter Extraction..." << std::endl;
            Router router;
            router.addRoute(HttpMethod::GET, "/users/:id", make_factory("user_detail"));
            router.addRoute(HttpMethod::GET,
                            "/posts/:postId/comments/:commentId",
                            make_factory("comment_detail"));

            assert_match(router, HttpMethod::GET, "/users/123", "user_detail");
            assert_param(router, HttpMethod::GET, "/users/123", "id", "123");

            assert_match(router, HttpMethod::GET, "/posts/abc/comments/99", "comment_detail");
            assert_param(router, HttpMethod::GET, "/posts/abc/comments/99", "postId", "abc");
            assert_param(router, HttpMethod::GET, "/posts/abc/comments/99", "commentId", "99");
        }

        // Test 4: 最具体路由优先 (Specificity Priority)
        // 验证静态路径优先于参数路径匹配 (Static > Param)
        {
            std::cout << "Test 4: Specificity Priority (Static > Param)..." << std::endl;
            Router router;
            // 注册一个通用参数路由
            router.addRoute(HttpMethod::GET, "/users/:id", make_factory("generic_user"));
            // 注册一个特定静态路由
            router.addRoute(HttpMethod::GET, "/users/new", make_factory("create_user_form"));

            // 请求 "new" 应该命中静态路由，而不是被参数路由捕获为 id="new"
            assert_match(router, HttpMethod::GET, "/users/new", "create_user_form");

            // 其他值应命中参数路由
            assert_match(router, HttpMethod::GET, "/users/john", "generic_user");
        }

        // Test 5: 通配符与 Fallback (Wildcard & Fallback)
        // 验证 * 通配符的行为以及未匹配时的空返回
        {
            std::cout << "Test 5: Wildcard & Fallback..." << std::endl;
            Router router;
            router.addRoute(HttpMethod::GET, "/static/*", make_factory("static_file"));
            router.addRoute(HttpMethod::GET, "/api/v1/users", make_factory("users_list"));

            // 匹配通配符
            assert_match(router, HttpMethod::GET, "/static/css/style.css", "static_file");
            assert_param(router, HttpMethod::GET, "/static/css/style.css", "*", "css/style.css");

            // 未匹配路径 (Fallback)
            assert_match(router, HttpMethod::GET, "/api/v1/unknown", nullptr);
            assert_match(router, HttpMethod::GET, "/random", nullptr);
        }

        // Test 6: 复杂混合场景 (Complex Mixed Scenario)
        // 验证多层级、混合参数和静态段的正确解析
        {
            std::cout << "Test 6: Complex Mixed Scenario..." << std::endl;
            Router router;
            router.addRoute(HttpMethod::GET, "/", make_factory("root"));
            router.addRoute(HttpMethod::GET, "/a/b/c", make_factory("deep_static"));
            router.addRoute(HttpMethod::GET, "/a/:param/c", make_factory("middle_param"));

            assert_match(router, HttpMethod::GET, "/", "root");
            assert_match(router, HttpMethod::GET, "/a/b/c", "deep_static");
            assert_match(router, HttpMethod::GET, "/a/xyz/c", "middle_param");

            // 路径前缀匹配但不完整的情况
            assert_match(router, HttpMethod::GET, "/a/b", nullptr);
        }

        std::cout << "\nAll tests passed successfully." << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}