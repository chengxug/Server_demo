#include "../HttpParser.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

// Mock Callback: 用于捕获 Parser 解析出的数据
struct ParsedData
{
    std::string method;
    std::string path;
    std::string version;
    Headers     headers;
    std::string body;
    bool        headers_complete = false;
    bool        message_complete = false;
    int         error_code = 0;

    void reset()
    {
        method.clear();
        path.clear();
        version.clear();
        headers.clear();
        body.clear();
        headers_complete = false;
        message_complete = false;
        error_code = 0;
    }
};

class MockParserCallback : public HttpParserCallback
{
public:
    ParsedData& data_;

    explicit MockParserCallback(ParsedData& data)
        : data_(data)
    {
    }

    void onRequestLine(const std::string& method, const std::string& path,
                       const std::string& version) override
    {
        data_.method = method;
        data_.path = path;
        data_.version = version;
    }

    void onHeader(const std::string& name, const std::string& value) override
    {
        data_.headers[name] = value;
    }

    void onHeadersComplete() override { data_.headers_complete = true; }

    void onBody(const char* data, size_t len) override { data_.body.append(data, len); }

    void onMessageComplete() override { data_.message_complete = true; }

    void onError(int code) override { data_.error_code = code; }
};

// 辅助函数：简化 feed 调用
void feed(HttpParser& parser, const std::string& data)
{
    parser.feed(data.c_str(), data.size());
}

// 辅助函数，断言string相等
void assert_eq(const std::string& val1, const std::string& val2)
{
    if (val1 != val2)
    {
        std::cerr << "Assertion failed: \"" << val1 << "\" != \"" << val2 << "\"" << std::endl;
        std::exit(1);
    }
}

// 辅助函数，断言size_t相等
void assert_eq(const size_t val1, const size_t val2)
{
    if (val1 != val2)
    {
        std::cerr << "Assertion failed: " << val1 << " != " << val2 << std::endl;
        std::exit(1);
    }
}

// 辅助函数，断言条件为真
void assert_true(bool cond)
{
    if (!cond)
    {
        std::cerr << "Assertion failed: condition is false" << std::endl;
        std::exit(1);
    }
}

int main()
{
    try
    {
        // Test 1: Simple GET Request (No Body)
        // 验证标准 GET 请求在双 CRLF 后立即完成，且无 Body
        {
            std::cout << "Test 1: Simple GET Request (No Body)..." << std::endl;
            ParsedData         data;
            MockParserCallback callback(data);
            HttpParser         parser(&callback);

            // 1. Feed Request Line
            feed(parser, "GET /index.html HTTP/1.1\r\n");
            assert_eq(data.method, "GET");
            assert_eq(data.path, "/index.html");
            assert_eq(data.version, "HTTP/1.1");
            assert_true(!data.headers_complete);

            // 2. Feed Headers
            feed(parser, "Host: localhost\r\n");
            feed(parser, "User-Agent: Test\r\n");
            assert_eq(data.headers["Host"], "localhost");
            assert_eq(data.headers["User-Agent"], "Test");
            assert_true(!data.headers_complete);

            // 3. Feed End of Headers (CRLF)
            feed(parser, "\r\n");
            assert_true(data.headers_complete);
            assert_true(data.message_complete);   // 无 Content-Length，应立即完成
            assert_eq(data.body, "");
        }

        // Test 2: Request with Content-Length Body
        // 验证 Parser 能够精确读取 Content-Length 指定的字节数
        {
            std::cout << "Test 2: Request with Content-Length Body..." << std::endl;
            ParsedData         data;
            MockParserCallback callback(data);
            HttpParser         parser(&callback);

            std::string headers = "POST /api/data HTTP/1.1\r\n"
                                  "Content-Length: 5\r\n"
                                  "\r\n";
                                  
            feed(parser, headers);
            assert_true(data.headers_complete);
            assert_true(!data.message_complete);   // Body 尚未接收

            // Feed partial body
            feed(parser, "Hel");
            assert_eq(data.body, "Hel");
            assert_true(!data.message_complete);

            // Feed remaining body
            feed(parser, "lo");
            assert_eq(data.body, "Hello");
            assert_true(data.message_complete);
        }

        // Test 3: Split Input at Critical Boundaries
        // 验证 Parser 在数据包被切割在尴尬位置（如 CRLF 中间）时的鲁棒性
        {
            std::cout << "Test 3: Split Input at Critical Boundaries..." << std::endl;
            ParsedData         data;
            MockParserCallback callback(data);
            HttpParser         parser(&callback);

            // Split inside Request Line
            feed(parser, "GE");
            assert_eq(data.method, "");
            feed(parser, "T /abc HT");
            assert_eq(data.method, "");
            feed(parser, "TP/1.1\r");   // Split CRLF (\r without \n)
            assert_eq(data.method, "");
            feed(parser, "\n");   // Complete CRLF
            assert_eq(data.method, "GET");
            assert_eq(data.path, "/abc");

            // Split inside Header delimiter
            feed(parser, "Host: 127.0.0.1\r");
            assert_true(data.headers.find("Host") == data.headers.end());
            feed(parser, "\n");
            assert_eq(data.headers["Host"], "127.0.0.1");

            // Split inside Empty Line (End of Headers)
            feed(parser, "\r");
            assert_true(!data.headers_complete);
            feed(parser, "\n");
            assert_true(data.headers_complete);
            assert_true(data.message_complete);
        }

        // Test 4: Incomplete Body (Underflow)
        // 验证当 Body 数据不足 Content-Length 时，Parser 不会提前完成
        {
            std::cout << "Test 4: Incomplete Body (Underflow)..." << std::endl;
            ParsedData         data;
            MockParserCallback callback(data);
            HttpParser         parser(&callback);

            feed(parser, "PUT /upload HTTP/1.1\r\nContent-Length: 10\r\n\r\n");
            assert_true(data.headers_complete);

            feed(parser, "12345");
            assert_eq(data.body.size(), 5);
            assert_true(!data.message_complete);

            feed(parser, "6789");   // 总共 9 字节，仍缺 1 字节
            assert_eq(data.body.size(), 9);
            assert_true(!data.message_complete);

            feed(parser, "0");   // 补齐最后 1 字节
            assert_true(data.message_complete);
        }

        // Test 5: Header Value Trimming
        // 验证 Header 值的前导空格被正确去除
        {
            std::cout << "Test 5: Header Value Trimming..." << std::endl;
            ParsedData         data;
            MockParserCallback callback(data);
            HttpParser         parser(&callback);

            feed(parser, "GET / HTTP/1.1\r\n");
            feed(parser, "Key-1:Value1\r\n");       // 无空格
            feed(parser, "Key-2: Value2\r\n");      // 标准空格
            feed(parser, "Key-3:    Value3\r\n");   // 多余空格
            feed(parser, "\r\n");

            assert_eq(data.headers["Key-1"], "Value1");
            assert_eq(data.headers["Key-2"], "Value2");
            assert_eq(data.headers["Key-3"], "Value3");
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