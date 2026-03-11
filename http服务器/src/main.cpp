#include<iostream>
#include"server.h"

int main(int argc,char* argv[])
{
    std::cerr << "Server main() started" << std::endl;
    try
    {
        if(argc!=4)
        {
            std::cerr << "用法: " << argv[0] << " <监听地址> <端口> <线程数>\n"
                      << "例如: " << argv[0] << " 0.0.0.0 8080 4\n";
            return 1;
        }
        std::string addr=argv[1];
        unsigned short port=static_cast<unsigned short>(std::atoi(argv[2]));
        int thread_count=std::atoi(argv[3]);

        MultiThreadHttpServer server(addr,port,thread_count);
        std::cout << "服务器启动在 " << addr << ":" << port
                  << "，工作线程数: " << thread_count << std::endl;
        server.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << "异常："<<e.what() << std::endl;
        return 1;
    }
    return 0;
}