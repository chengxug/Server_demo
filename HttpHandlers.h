#pragma once
#include "HttpRouter.h"
#include "Logger.h"
#include <cstring>
#include <fstream>

// html文件处理器
class HtmlFileHandler : public RequestHandler
{
public:
    HtmlFileHandler(const std::string& file_path, std::shared_ptr<spdlog::logger> logger)
        : file_path_(file_path)
        , logger_(logger)
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
            logger_->warn("[405] Method Not Allowed: {}", request.path);
            return;
        }

        std::ifstream file(file_path_);
        if (!file.is_open())
        {
            response_.status_code = 500;
            response_.status_message = "Internal Server Error";
            response_.headers["Content-Length"] = "0";
            logger_->error("Failed to open file: {}", file_path_);
            return;
        }

        char buffer[1024];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
        {
            response_.body.append(buffer, file.gcount());
        }

        response_.status_code = 200;
        response_.status_message = "OK";
        response_.headers["Content-Type"] = "text/html";
        response_.headers["Content-Length"] = std::to_string(response_.body.size());
    }

    void           onBody(const char*, size_t) override {}
    void           onEOM() override {}
    HttpResponse&& takeResponse() override { return std::move(response_); }

private:
    std::string                     file_path_;
    std::shared_ptr<spdlog::logger> logger_;
};
