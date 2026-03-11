#include<boost/asio.hpp>
#include<memory>
#include<vector>
#include<atomic>
using std::string;

class RequestHandler;
class WebSocketRoom;

class MultiThreadHttpServer{ //多线程HTTP服务器类
public:
    MultiThreadHttpServer(const string&address,unsigned short port,int thread_count);
    ~MultiThreadHttpServer();
    void run(); //启动服务器，阻塞直到停止
    void stop();//停止服务器
private:
    void do_accept();
    void handle_accept(boost::system::error_code ec,boost::asio::ip::tcp::socket socket);

    boost::asio::io_context main_ioc;//主I/O，只处理accept
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::unique_ptr<boost::asio::io_context>>worker_iocs;//工作线程的ioc
    std::vector<std::thread>threads; //工作线程
    std::atomic<size_t>next_index; //轮询索引
    int thread_count; //线程数

    std::shared_ptr<RequestHandler>handler; //http请求处理器
    std::shared_ptr<WebSocketRoom>ws_room; //websocket聊天室
    std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guards;
};