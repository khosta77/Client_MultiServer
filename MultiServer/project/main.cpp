#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <exception>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

std::atomic<size_t> i = 0;

class MyException : public std::exception
{
public:
    explicit MyException( const std::string& msg ) : m_msg(msg) {}
    const char *what() const noexcept override { return m_msg.c_str(); }

private:
    std::string m_msg;
};

class ServerException : public MyException
{
public:
    ServerException( const std::string& m ) : MyException(m) {}
};

class Server
{
private:
    //std::vector<int> sock;
    int listener;
    struct sockaddr_in addr;

    void socketInit()
    {
        listener = socket( AF_INET, SOCK_STREAM, 0 );
        if( listener < 0 )
            throw ServerException( ( "listener = " + std::to_string(listener) ) );

    }

    void fillAddress( const std::string& IP, const int& PORT )
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        addr.sin_addr.s_addr = inet_addr(IP.c_str());
    }

    void bindInit()
    {
        int b = bind( listener, ( struct sockaddr* )&addr, sizeof(addr) );
        if( b < 0 )
            throw ServerException( ( "bind = " + std::to_string(b) ) );
        listen( listener, 1 );
    }

    int sockAccept()
    {
        int sock = accept( listener, NULL, NULL );
        if( sock < 0 )
            throw ServerException( "Sock no accept" );
        return sock;
    }

    std::string readFromRecv( const int& s )
    {
        std::string receivedData;
        char *buffer = new char[1024];
        while(true)
        {
            int bytesReceived = recv( s, buffer, 1024, 0 );
            if( bytesReceived <= 0 )
            {
                delete []buffer;
                receivedData.clear();
                return "";
            }
            receivedData.append( buffer, bytesReceived );
            if( receivedData.back() == ';' )
                break;
        }
        delete []buffer;
        return receivedData;
    }

    void sendToSock( const int& s, const std::string& msg )
    {
        const char* dataPtr = msg.c_str();
        size_t dataSize = msg.length();
        size_t totalSent = 0;
        while( totalSent < dataSize )
        {
            int bytesSent = send( s, ( dataPtr + totalSent ), ( dataSize - totalSent ), 0 );
            if( bytesSent == -1 )
                break;
            totalSent += bytesSent;
        }
    }


    void process( const int& s )
    {
        while(true)
        {
            std::string content = readFromRecv(s);
            if( content.empty() )
                break;
            sendToSock( s, ( std::to_string(i++) + "|:" + content ) );
        }
        std::cout << "sock close = " << s << std::endl;
        close(s);
    }

public:
    Server( const std::string& IP, const int& PORT )
    {
        socketInit();
        fillAddress( IP, PORT );
        bindInit();
        std::cout << "server in system address: " << IP << ":" << PORT << std::endl;
    }

    int run()
    {
        for( ;; )
        {
            int sock = sockAccept();
            std::cout << "--->sock:= " << sock << std::endl;
            std::thread t( &Server::process, this, sock );
            t.detach(); 
        }
        return 0;
    }

};  // Server

int main()
{
    Server server( "127.0.0.1", 8000 );
    return server.run();
}


