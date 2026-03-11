#include "websocket_room.h"
#include "session.h"
#include<iostream>

void WebSocketRoom::join(std::shared_ptr<WebSocketSession> session) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "[WebSocketRoom] join, session=" << session.get() << std::endl; 
    sessions.insert(session);
}

void WebSocketRoom::leave(std::shared_ptr<WebSocketSession> session)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "[WebSocketRoom] leave, session=" << session.get() << std::endl; 
    sessions.erase(session);
}

void WebSocketRoom::broadcast(const std::string& message)
{
    std::lock_guard<std::mutex>lock(mutex_);
    for(auto& session:sessions)
    {
        session->send(message);
    }
}