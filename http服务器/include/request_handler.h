#include <boost/beast/http.hpp>
#include <functional>
#include <string>
#include <unordered_map>

namespace http = boost::beast::http;

//http请求处理器类：支持路由和静态服务
class RequestHandler
{
public:
    using HandlerFunc = std::function<http::response<http::string_body>(const http::request<http::string_body>&)>;
    void add_route(const std::string& path,HandlerFunc handler);
    //处理请求：先匹配路由，再尝试静态文件
    http::response<http::string_body>handle_request(const http::request<http::string_body>&req);
private:
    http::response<http::string_body>serve_static_file(const http::request<http::string_body>&req);
    http::response<http::string_body> error_response(http::status status, const std::string& message, unsigned version);
    std::string get_mime_type(const std::string&path);
    std::unordered_map<std::string,HandlerFunc>routes;
    static const std::string doc_root; //静态文件根目录
};