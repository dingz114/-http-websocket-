#include "server.h"
#include "request_handler.h"
#include "session.h"
#include "websocket_room.h"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include<iostream>

MultiThreadHttpServer::MultiThreadHttpServer(const std::string& address, unsigned short port, int thread_count)
    : acceptor_(main_ioc),thread_count(thread_count),handler(std::make_shared<RequestHandler>()),ws_room(std::make_shared<WebSocketRoom>()) 
{
    handler->add_route("/hello", [](const http::request<http::string_body>& req) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, "application/json");
        res.body() = "{\"message\":\"Hello from custom route!\"}\n";
        res.prepare_payload();
        return res;
    });
    // 设置acceptor
    tcp::endpoint endpoint(asio::ip::make_address(address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    // 创建工作线程的io_context
    for (int i = 0; i < thread_count; ++i) {
        auto ioc = std::make_unique<asio::io_context>(1);
        // 创建 work_guard 防止 run() 立即返回
        work_guards.emplace_back(asio::make_work_guard(*ioc));
        worker_iocs.push_back(std::move(ioc));
    }
    next_index = 0;
}

MultiThreadHttpServer::~MultiThreadHttpServer() {
    stop();
}

void MultiThreadHttpServer::run() {
    std::cerr << "[Main] run() started" << std::endl;
    do_accept();
    std::cerr << "[Main] do_accept() called" << std::endl;
    //启动工作线程
    std::cerr << "[Main] Creating " << thread_count << " worker threads" << std::endl;  
    // for(auto&ioc:worker_iocs)
    // {
    //     threads.emplace_back([&ioc](){
    //         ioc->run();
    //     });
    // }
    for (size_t i = 0; i < worker_iocs.size(); ++i) {
        threads.emplace_back([this, i] {
            std::cerr << "[Worker " << i << "] thread started" << std::endl;   // 新增
            worker_iocs[i]->run();
            std::cerr << "[Worker " << i << "] thread stopped" << std::endl;   // 新增
        });
    }
    //主线程运行main_ioc处理accept
    std::cerr << "[Main] entering main_ioc_.run()" << std::endl;
    main_ioc.run();
    std::cerr << "[Main] main_ioc_.run() exited" << std::endl;
    //等待所有工作进程结束
    for(auto&t:threads)
    {
        if(t.joinable())
        {
            t.join();
        }
    }
    std::cerr << "[Main] all threads joined" << std::endl;  
}

void MultiThreadHttpServer::stop() {
    work_guards.clear(); 
    // 停止所有io_context
    main_ioc.stop();
    for (auto& ioc : worker_iocs) {
        ioc->stop();
    }
}

void MultiThreadHttpServer::do_accept() {
    std::cerr << "[Main] do_accept() called" << std::endl;
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            std::cerr << "[Main] handle_accept triggered, ec=" << ec.message() << std::endl;
            handle_accept(ec, std::move(socket));
        });
}

void MultiThreadHttpServer::handle_accept(boost::system::error_code ec, tcp::socket socket) {
    if(!ec)
    {
        std::cerr << "[Main] accept success, socket fd=" << socket.native_handle() << std::endl;
        //选择下一个工作线程的ioc(轮询)
        auto& next_ioc=*worker_iocs[next_index];
        next_index=(next_index+1)%thread_count;
        //将socket移交给下一个io_context
        asio::post(next_ioc,[this,s=std::move(socket)]()mutable{
            std::cerr << "[Worker] processing new connection" << std::endl; 
            std::make_shared<HttpSession>(std::move(s),handler,ws_room)->start();
        });
    }
    else 
    {
        std::cerr<<"accept error:"<<ec.message()<<std::endl;
    }
    do_accept();//继续接受下一个连接
}