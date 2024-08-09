#include <iostream>
#include <algorithm>
#include <vector>
#include <set>
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
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

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
    int _listener;
    struct sockaddr_in _addr;

    int sockAccept()
    {
        int sock = accept( _listener, NULL, NULL );
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
        _listener = socket( AF_INET, SOCK_STREAM, 0 );
        if( _listener < 0 )
            throw ServerException( ( "listener = " + std::to_string(_listener) ) );
        fcntl( _listener, F_SETFL, O_NONBLOCK );

        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(PORT);
        _addr.sin_addr.s_addr = inet_addr(IP.c_str());

        if( int _bind = bind( _listener, ( struct sockaddr* )&_addr, sizeof(_addr) ); _bind < 0 )
            throw ServerException( ( "bind = " + std::to_string(_bind) ) );
        listen( _listener, 1 );

        std::cout << "server in system address: " << IP << ":" << PORT << std::endl;
    }

    int run()
    {
        std::set<int> clients;

        while( true )
        {
            // Заполняем множество сокетов
            fd_set readset;
            FD_ZERO( &readset );
            FD_SET( _listener, &readset );

            for( auto it = clients.begin(); it != clients.end(); it++ )
                FD_SET( *it, &readset );

            // Задаём таймаут
            timeval timeout;
            timeout.tv_sec = 15;
            timeout.tv_usec = 0;

            // Ждём события в одном из сокетов
            int mx = std::max( _listener, *max_element( clients.begin(), clients.end() ) );
            if( int s = select( ( mx + 1 ), &readset, NULL, NULL, &timeout ); s <= 0 )  // Вот тут собака зарыта
                throw ServerException( ( "select " + std::to_string(s) ) );

            // Определяем тип события и выполняем соответствующие действия
            if( FD_ISSET( _listener, &readset ) )
            {
                if( int sock = accept( _listener, NULL, NULL ); sock >= 0 )
                {
                    fcntl( sock, F_SETFL, O_NONBLOCK );
                    clients.insert(sock);
                }
                else
                    throw ServerException( "Sock no accept" );
            }

            for( auto it = clients.begin(); it != clients.end(); it++ )
            {
                if( FD_ISSET( *it, &readset ) )
                {
                    std::string content = readFromRecv( *it );
                    if( content.empty() )
                    {
                        std::cout << "socket " << *it << " free" << std::endl;
                        close(*it);
                        clients.erase(*it);
                        break;
                    }
                    sendToSock( *it, ( std::to_string(i++) + "|:" + content ) );
                }
            }
        }
        return 0;
    }

};  // Server

int main()
{
    Server server( "127.0.0.1", 8000 );
    return server.run();
}


