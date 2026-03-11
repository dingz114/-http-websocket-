#include "session.h"
#include "request_handler.h"
#include "websocket_room.h"
#include <boost/beast/version.hpp>
#include <iostream>
#include <boost/algorithm/string.hpp>

HttpSession::HttpSession(tcp::socket socket,std::shared_ptr<RequestHandler> handler,std::shared_ptr<WebSocketRoom> ws_room)
    : socket_(std::move(socket)), handler(handler), ws_room(ws_room) {}

void HttpSession::start()
{
    do_read();
}

void HttpSession::do_read()
{
    auto self=shared_from_this();
    http::async_read(socket_,buffer,request_,[self](beast::error_code ec,std::size_t transfered)
    {
        self->on_read(ec,transfered);
    });
}

void HttpSession::on_read(beast::error_code ec,std::size_t /*transfered*/)
{
    std::cerr << "[HttpSession] on_read, ec=" << ec.message() << std::endl;
    if(ec==http::error::end_of_stream)
    {
        std::cerr << "[HttpSession] end_of_stream" << std::endl;
        //正常关闭
        socket_.shutdown(tcp::socket::shutdown_send,ec);
        return;
    }
    if(ec)
    {
        std::cerr << "[HttpSession] read error: " << ec.message() << std::endl;
        return; //发生错误，关闭连接
    }
    std::cerr << "[HttpSession] received request for " << request_.target() << std::endl;
    handle_request();
}

void HttpSession::handle_request()
{
    std::cerr << "[HttpSession] handle_request start" << std::endl; 
    //检查是否为websocket升级请求
    if(request_.find(http::field::upgrade)!=request_.end()&&
        boost::algorithm::iequals(request_[http::field::upgrade],"websocket"))
    {
        std::cerr << "[HttpSession] upgrading to WebSocket" << std::endl;
        //移交socket给WebsocketSession
        auto ws_session=std::make_shared<WebSocketSession>(std::move(socket_),ws_room,std::move(request_));
        std::cerr << "[HttpSession] WebSocketSession created, calling start()" << std::endl;
        ws_session->start();
        return; //当前httpSession对象将被销毁
    }
    //普通http请求
    std::cerr << "[HttpSession] calling handler_->handle_request" << std::endl;
    auto res=handler->handle_request(request_);
    res.set(http::field::server,"MyCppServer");
    res.keep_alive(request_.keep_alive());
    std::cerr << "[HttpSession] sending response" << std::endl;
    send_response(std::move(res));
}

void HttpSession::send_response(http::response<http::string_body> res) {
    std::cerr << "[HttpSession] send_response, keep_alive=" << res.keep_alive() << std::endl; 
    auto self=shared_from_this();
    bool keep_alive=res.keep_alive();
    auto res_ptr = std::make_shared<http::response<http::string_body>>(std::move(res));
    http::async_write(socket_, *res_ptr,
        [self, keep_alive, res_ptr](beast::error_code ec, std::size_t bytes_transferred) {
            std::cerr << "[HttpSession] async_write completed, ec=" << ec.message() << std::endl;
            if (!ec) {
                if (keep_alive) {
                    std::cerr << "[HttpSession] keep-alive, reset and read next" << std::endl;
                    self->request_ = {};
                    self->buffer.consume(self->buffer.size());
                    self->do_read();
                } else {
                    std::cerr << "[HttpSession] close connection" << std::endl;
                    self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                }
            } else {
                std::cerr << "[HttpSession] write error: " << ec.message() << std::endl;
            }
        });
    std::cerr << "[HttpSession] async_write posted" << std::endl;
}

std::atomic<int> WebSocketSession::next_id_{1};

WebSocketSession::WebSocketSession(tcp::socket socket, std::shared_ptr<WebSocketRoom> room,http::request<http::string_body> req)
    : ws(std::move(socket)), room(room),request_(std::move(req)) {
        id_ = "user" + std::to_string(next_id_++);
        std::cerr << "[WebSocketSession] constructed, this=" << this << std::endl; 
    }

WebSocketSession::~WebSocketSession() {
    std::cerr << "[WebSocketSession] destroyed, this=" << this << std::endl;
}

void WebSocketSession::start() {
    std::cerr << "[WebSocketSession] start() called, this=" << this << std::endl;

    ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "MyCppServer");
        }));

    auto self = shared_from_this();
    ws.async_accept(request_,
        [self](beast::error_code ec) {
            std::cerr << "[WebSocketSession] async_accept callback ENTERED, ec=" << ec.message() << std::endl;
            if (!ec) {
                std::cerr << "[WebSocketSession] accept success, joining room" << std::endl;
                self->room->join(self);
                self->do_read();
            } else {
                std::cerr << "[WebSocketSession] accept error: " << ec.message() << std::endl;
            }
        });
    std::cerr << "[WebSocketSession] async_accept posted" << std::endl;  // 确认操作已发起
}

void WebSocketSession::send(const std::string& message) {
    std::cerr << "[WebSocketSession] send: " << message << std::endl;
    // 同步发送（简单实现，最好改为异步+队列）
    boost::system::error_code ec;
    ws.write(asio::buffer(message), ec);
    if (ec) {
        std::cerr << "[WebSocketSession] send error: " << ec.message() << std::endl;
    }
}

void WebSocketSession::do_read() {
    std::cerr << "[WebSocketSession] do_read()" << std::endl;
    auto self=shared_from_this();
    ws.async_read(buffer,[self](beast::error_code ec,std::size_t bytes){
        std::cerr << "[WebSocketSession] async_read callback, ec=" << ec.message() << ", bytes=" << bytes << std::endl;
        self->on_read(ec,bytes);
    });
}

void WebSocketSession::on_read(beast::error_code ec,std::size_t bytes)
{
    std::cerr << "[WebSocketSession] on_read, ec=" << ec.message() << std::endl;
    if(!ec)
        {
            //将收到的消息转为字符串
            std::string msg=beast::buffers_to_string(buffer.data());
            std::cerr << "[WebSocketSession] received message: " << msg << std::endl;
            // 添加ID前缀
            std::string broadcast_msg = "[" + id_ + "] " + msg;
            std::cerr << "[WebSocketSession] broadcasting: " << broadcast_msg << std::endl; 
            //广播给房间内所有客户端
            room->broadcast(broadcast_msg);
            //清空缓冲区，准备读取下一条消息
            buffer.consume(buffer.size());
            //继续读取
            do_read();
        }
        else if(ec==websocket::error::closed)
        {
            std::cerr << "[WebSocketSession] connection closed" << std::endl;
            //客户端主动关闭连接
            room->leave(shared_from_this());
        }
        else
        {
            std::cerr << "[WebSocketSession] read error: " << ec.message() << std::endl;
            //其他错误
            room->leave(shared_from_this());
        }
}