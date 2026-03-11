#include "request_handler.h"
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>

namespace fs = boost::filesystem;

const std::string RequestHandler::doc_root = "../www";

void RequestHandler::add_route(const std::string& path, HandlerFunc handler) {
    routes[path] = std::move(handler);
}

http::response<http::string_body> RequestHandler::handle_request(const http::request<http::string_body>& req)
{
    std::cerr << "[RequestHandler] handling request for " << req.target() << std::endl; 
    //先检查自定义路由
    auto it=routes.find(std::string(req.target()));
    if(it!=routes.end())
    {
        std::cerr << "[RequestHandler] custom route matched" << std::endl;
        return it->second(req);
    }
    //再尝试静态文件
    std::cerr << "[RequestHandler] serving static file" << std::endl;
    return serve_static_file(req);
}

http::response<http::string_body> RequestHandler::serve_static_file(const http::request<http::string_body>& req)
{
    std::cerr << "[RequestHandler] serve_static_file start" << std::endl;
    //构建文件路径
    fs::path doc_root_=fs::current_path()/doc_root;
    fs::path request_path=std::string(req.target());
    if(request_path=="/")
    {
        request_path="/index.html"; //默认首页
    }
    fs::path full_path=doc_root_/request_path.relative_path();
    std::cerr << "[RequestHandler] full_path = " << full_path << std::endl; 
    try{
        full_path=fs::weakly_canonical(full_path);
        std::cerr << "[RequestHandler] full_path after canonical = " << full_path << std::endl;
    }
    catch(const fs::filesystem_error&)
    {
        std::cerr << "[RequestHandler] path canonical error" << std::endl;
        return error_response(http::status::bad_request,"invalid path",req.version());
    }
    //检查文件是否存在且在根目录下
    // 对 doc_root 也进行规范化，以便比较
    fs::path doc_root_canonical = fs::weakly_canonical(doc_root_);
    std::cerr << "[RequestHandler] doc_root_canonical = " << doc_root_canonical << std::endl;
    std::cerr << "[RequestHandler] full_path.string() = " << full_path.string() << std::endl;
    std::cerr << "[RequestHandler] find result = " << full_path.string().find(doc_root_canonical.string()) << std::endl;
    if (!fs::exists(full_path) || fs::is_directory(full_path) ||
        full_path.string().find(doc_root_canonical.string()) != 0) {
        std::cerr << "[RequestHandler] file not found or not in doc root" << std::endl;
        return error_response(http::status::not_found, "File not found", req.version());
    }
    //读取文件内容
    std::ifstream file(full_path.string(),std::ios::binary);
    if(!file)
    {
        std::cerr << "[RequestHandler] cannot open file" << std::endl;
        return error_response(http::status::internal_server_error, "Cannot open file", req.version());
    }
    std::string content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());

    http::response<http::string_body> res{http::status::ok,req.version()};
    res.set(http::field::content_type,get_mime_type(full_path.string()));
    res.body()=std::move(content);
    res.prepare_payload();
    std::cerr << "[RequestHandler] file served, size=" << content.size() << std::endl;
    return res;
}

http::response<http::string_body> RequestHandler::error_response(http::status status, const std::string& message, unsigned version)
{
    http::response<http::string_body>res{status,version};
    res.set(http::field::content_type,"text/palin");
    res.body()=message+"\n";
    res.prepare_payload();
    return res;
}

std::string RequestHandler::get_mime_type(const std::string& path) 
{
    static std::unordered_map<std::string,std::string>mime={
        {".html", "text/html"}, {".htm", "text/html"},
        {".css", "text/css"}, {".js", "text/javascript"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".txt", "text/plain"}, {".json", "application/json"}
    };
    auto ext=fs::extension(path);
    auto it=mime.find(ext);
    return it!=mime.end()?it->second:"application/octet-stream";
}