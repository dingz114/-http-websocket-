#include<boost/asio.hpp>
#include<boost/beast/core.hpp>
#include<boost/beast/http.hpp>
#include<boost/beast/websocket.hpp>
#include<memory>

namespace beast=boost::beast;
namespace http=beast::http;
namespace websocket=beast::websocket;
namespace asio=boost::asio;
using tcp=asio::ip::tcp;

class RequestHandler;
class WebSocketRoom;

//http会话类：处理http请求
class HttpSession:public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(tcp::socket socket,std::shared_ptr<RequestHandler>handler,std::shared_ptr<WebSocketRoom>ws_room);
    void start();
private:
    void do_read();
    void on_read(beast::error_code ec,std::size_t transfered);
    void handle_request();
    void send_response(http::response<http::string_body>res);

    tcp::socket socket_;
    beast::flat_buffer buffer;
    http::request<http::string_body>request_;
    std::shared_ptr<RequestHandler>handler;
    std::shared_ptr<WebSocketRoom>ws_room;
};

//websocket类：处理websocket连接
class WebSocketSession:public std::enable_shared_from_this<WebSocketSession>{
public:
    WebSocketSession(tcp::socket socket,std::shared_ptr<WebSocketRoom>room,http::request<http::string_body> req);
    ~WebSocketSession();
    void start();
    void send(const std::string& message);
    const std::string& id() const { return id_; } 
private:
    void do_read();
    void on_read(beast::error_code ec,std::size_t bytes);

    websocket::stream<tcp::socket>ws;
    beast::flat_buffer buffer;
    std::shared_ptr<WebSocketRoom>room;
    http::request<http::string_body> request_; 
    std::string id_;
    static std::atomic<int> next_id_;
};

