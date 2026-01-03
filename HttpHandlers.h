#pragma once
#include "HttpRouter.h"
#include "Logger.h"
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <stack>
#include <vector>

// html文件处理器
class HtmlFileHandler : public RequestHandler
{
public:
    HtmlFileHandler(const std::string& file_path)
        : file_path_(file_path)
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
            // 使用统一的 g_logger
            if (g_logger)
                g_logger->warn("[405] Method Not Allowed: {}", request.path);
            return;
        }

        std::ifstream file(file_path_);
        if (!file.is_open())
        {
            response_.status_code = 500;
            response_.status_message = "Internal Server Error";
            response_.headers["Content-Length"] = "0";
            if (g_logger)
                g_logger->error("Failed to open file: {}", file_path_);
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
    std::string file_path_;
};

/**
 * helper function for register global router
 * @details 扫描指定目录及其子目录，获取所有HTML文件的路径
 *
 * @param directory 目录路径
 *
 * @return HTML文件路径列表
 */
std::vector<std::string> get_html_files_recursively(const std::string& directory)
{
    std::vector<std::string> html_files;
    std::stack<std::string>  dirs;
    dirs.push(directory);

    while (!dirs.empty())
    {
        std::string cur_dir = dirs.top();
        dirs.pop();

        DIR* dir = opendir(cur_dir.c_str());
        if (dir == nullptr)
        {
            if (g_logger)
                g_logger->error("Failed to open directory: {}", cur_dir);
            continue;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;   // Skip . and ..
            }
            std::string full_path = cur_dir + "/" + entry->d_name;
            if (entry->d_type == DT_DIR)
            {
                dirs.push(full_path);   // Push subdirectory to stack
            }
            else if (entry->d_type == DT_REG)
            {
                if (full_path.size() >= 5 && full_path.substr(full_path.size() - 5) == ".html")
                {
                    html_files.push_back(full_path);
                }
            }
        }
        closedir(dir);
    }
    return html_files;
}

/**
 * 注册全局路由，扫描指定目录下的所有 HTML 文件并注册为静态路由
 * @return 路由对象指针
 */
std::unique_ptr<Router> register_grouter(std::string web_inf_dir)
{
    std::vector<std::string> html_files = get_html_files_recursively(web_inf_dir);
    std::unique_ptr<Router>  router(new Router());
    router->addRoute(HttpMethod::GET,
                     "/",
                     [web_inf_dir]() {
                         return std::unique_ptr<RequestHandler>(
                             new HtmlFileHandler(web_inf_dir + "/index.html"));
                     });
    for (const auto& file_path : html_files)
    {
        router->addRoute(HttpMethod::GET,
                         file_path.substr(web_inf_dir.size()),   // 去掉前缀目录
                         [file_path]() {
                             return std::unique_ptr<RequestHandler>(new HtmlFileHandler(file_path));
                         });
    }
    return router;
}