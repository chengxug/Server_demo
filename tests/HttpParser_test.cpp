#include "../HttpParser.h"
#include <cassert>
#include <iostream>

struct HttpRequest
{
    std::string method;
    std::string uri;
    std::string version;
    Headers     headers;
    std::string body;
};

class TestCallback : public HttpParserCallback
{
public:
    void onRequestLine(const std::string& method, const std::string& path,
                       const std::string& version) override
    {
        req.method = method;
        req.uri = path;
        req.version = version;
        request_line_called = true;
    }

    void onHeader(const std::string& name, const std::string& value) override
    {
        // 记录最后一个 header，证明该回调被触发
        last_header = {name, value};
        header_called = true;
    }

    void onHeadersComplete() override
    {
        headers_complete_called = true;
    }

    void onBody(const char* data, size_t len) override
    {
        req.body.append(data, len);
        body_called = true;
    }

    void onMessageComplete() override { message_complete_called = true; }

    void onError(int code) override
    {
        error_called = true;
        error_code = code;
    }

    HttpRequest                         req;
    std::pair<std::string, std::string> last_header;
    bool                                request_line_called{false};
    bool                                header_called{false};
    bool                                headers_complete_called{false};
    bool                                body_called{false};
    bool                                message_complete_called{false};
    bool                                error_called{false};
    int                                 error_code{0};
    ~TestCallback() override = default;
};

int main()
{
    // 用例 1：合法 GET
    {
        const std::string raw_request = "GET / HTTP/1.1\r\n"
                                        "Host: example.com\r\n"
                                        "Content-Length: 0\r\n"
                                        "\r\n";

        TestCallback cb;
        HttpParser   parser(&cb);
        parser.feed(raw_request.c_str(), raw_request.size());

        assert(cb.request_line_called);
        assert(cb.headers_complete_called);
        assert(cb.message_complete_called);
        assert(!cb.error_called);
        assert(cb.req.method == "GET");
        assert(cb.req.uri == "/");
        assert(cb.req.version == "HTTP/1.1");
        assert(cb.req.body.empty());
    }

    // 用例 2：合法 POST，含 body
    {
        const std::string body = "field1=value1&field2=value2";
        const std::string raw_request = "POST /form HTTP/1.1\r\n"
                                        "Host: example.com\r\n"
                                        "Content-Length: 27\r\n"
                                        "Content-Type: application/x-www-form-urlencoded\r\n"
                                        "\r\n" +
                                        body;

        TestCallback cb;
        HttpParser   parser(&cb);
        parser.feed(raw_request.c_str(), raw_request.size());

        assert(cb.request_line_called);
        assert(cb.headers_complete_called);
        assert(cb.body_called);
        assert(cb.message_complete_called);
        assert(!cb.error_called);
        assert(cb.req.method == "POST");
        assert(cb.req.uri == "/form");
        assert(cb.req.body == body);
    }

    // 用例 3：非法请求行（缺少版本），应触发 onError
    {
        const std::string bad_request = "GET /only_method_and_uri\r\n"
                                        "Host: example.com\r\n"
                                        "\r\n";

        TestCallback cb;
        HttpParser   parser(&cb);
        parser.feed(bad_request.c_str(), bad_request.size());

        assert(cb.error_called);               // 期望触发错误
        assert(!cb.message_complete_called);   // 不应完成正常消息
    }

    std::cout << "Pass" << std::endl;
    return 0;
}