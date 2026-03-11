cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Boost库
find_package(Boost REQUIRED COMPONENTS system filesystem)

include_directories(include)

# 源文件列表
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/session.cpp
    src/request_handler.cpp
    src/websocket_room.cpp
)

add_executable(chat_server ${SOURCES})
target_link_libraries(chat_server Boost::system Boost::filesystem pthread)