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

    std::mutex _m_save;
    std::mutex _m_vector;

    std::vector<int> _socketsToClear;
    std::set<int> _clients;
    fd_set _readset;
    timeval _timeout;

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

#if 0
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
#endif
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
        
        // Задаём таймаут
        _timeout.tv_sec = 15;
        _timeout.tv_usec = 0;
    }

    ~Server()
    {
        _clients.clear();
    }


    void updateClientsInfo()
    {
        // Заполняем множество сокетов
        FD_ZERO( &_readset );
        FD_SET( _listener, &_readset );
        for( auto it = _clients.begin(); it != _clients.end(); it++ )
            FD_SET( *it, &_readset );
    }

    // Ждём события в одном из сокетов
    void waitEvent()
    {
        int mx = std::max( _listener, *max_element( _clients.begin(), _clients.end() ) );
        if( int s = select( ( mx + 1 ), &_readset, NULL, NULL, &_timeout ); s <= 0 )
            throw ServerException( ( "select " + std::to_string(s) ) );
    }

    void fillSockNew()
    {
        if( FD_ISSET( _listener, &_readset ) )
        {
            if( int sock = accept( _listener, NULL, NULL ); sock >= 0 )
            {
                fcntl( sock, F_SETFL, O_NONBLOCK );
                _clients.insert(sock);
                std::cout << "new socket init: " << sock << std::endl;
            }
            else
                throw ServerException( "Sock no accept" );
        }
    }

    void clearSocket( const int& sock )  // поток
    {
        std::cout << "socket " << sock << " free" << std::endl;
        close(sock);
        
        std::lock_guard<std::mutex> mtx1_lock(_m_save);
        {
            std::lock_guard<std::mutex> mtx2_lock(_m_vector);
            {
                _socketsToClear.push_back(sock);
            }
        }
    }

    void process( const int& sock )
    {
        std::string content = readFromRecv( sock );
        if( content.empty() )
            clearSocket( sock );
        else
            sendToSock( sock, ( std::to_string(i++) + "|:" + content ) );
    }

    // Определяем тип события и выполняем соответствующие действия
    void launchProcess()
    {
        for( auto it = _clients.begin(); it != _clients.end(); it++ )
        {
            if( FD_ISSET( *it, &_readset ) )
                process(*it);
        }
    }

    void clearUnusedSockets()  // Поток
    {
        std::lock_guard<std::mutex> mtx1_lock(_m_save);
        {
            std::lock_guard<std::mutex> mtx2_lock(_m_vector);
            {
                for( const auto& it : _socketsToClear )
                    _clients.erase(it);
                _socketsToClear.clear();
            }
        }
    }

    int run()
    {
        while( true )
        {
            updateClientsInfo();
            waitEvent();
            fillSockNew();
            launchProcess();
            clearUnusedSockets();
#if 0
            // Заполняем множество сокетов
            FD_ZERO( &_readset );
            FD_SET( _listener, &_readset );

            for( auto it = _clients.begin(); it != _clients.end(); it++ )
                FD_SET( *it, &_readset );


            // Ждём события в одном из сокетов
            int mx = std::max( _listener, *max_element( _clients.begin(), _clients.end() ) );
            if( int s = select( ( mx + 1 ), &_readset, NULL, NULL, &_timeout ); s <= 0 )
                throw ServerException( ( "select " + std::to_string(s) ) );

            // Определяем тип события и выполняем соответствующие действия
            if( FD_ISSET( _listener, &_readset ) )
            {
                if( int sock = accept( _listener, NULL, NULL ); sock >= 0 )
                {
                    fcntl( sock, F_SETFL, O_NONBLOCK );
                    std::cout << "socket init: " << sock << std::endl;
                    _clients.insert(sock);
                }
                else
                    throw ServerException( "Sock no accept" );
            }

            for( auto it = _clients.begin(); it != _clients.end(); it++ )
            {
                if( FD_ISSET( *it, &_readset ) )
                {
                    std::string content = readFromRecv( *it );
                    if( content.empty() )
                    {
                        std::cout << "socket " << *it << " free" << std::endl;
                        close(*it);
                        _socketsToClear.push_back(*it);
                    }
                    else
                    {
                        sendToSock( *it, ( std::to_string(i++) + "|:" + content ) );
                    }
                }
            }

            for( const auto& it : _socketsToClear )
                _clients.erase(it);
            _socketsToClear.clear();
#endif
        }
        return 0;
    }

};  // Server

int main()
{
    Server server( "127.0.0.1", 8000 );
    return server.run();
}


