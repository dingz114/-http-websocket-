#include <memory>
#include <set>
#include <mutex>

class WebSocketSession;

//websocket聊天室类：管理所有连接，广播消息
class WebSocketRoom:public std::enable_shared_from_this<WebSocketRoom>
{
public:
    void join(std::shared_ptr<WebSocketSession>session);
    void leave(std::shared_ptr<WebSocketSession>session);
    void broadcast(const std::string&message);
private:
    std::mutex mutex_;
    std::set<std::shared_ptr<WebSocketSession>>sessions;
};