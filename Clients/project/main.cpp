#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <exception>
#include <string>
#include <unordered_map>
#include <tuple>

struct PCout
{
    static std::mutex& Mutex()
    {
        static std::mutex mut;
        return mut;
    }
};

#define thread_cout(msg) { \
    PCout::Mutex().lock(); \
    msg; \
    PCout::Mutex().unlock(); \
}

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

class Client
{
private:
    int _socket;
    struct sockaddr_in _addr;    

    std::string readFromSock()
    {
        std::string receivedData;
        char *buffer = new char[1024];
        while(true)
        {
            int bytesReceived = recv( _socket, buffer, 1024, 0 );
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

    void sendToSock( const std::string& msg )
    {
        const char* dataPtr = msg.c_str();
        size_t dataSize = msg.length();
        size_t totalSent = 0;
        while( totalSent < dataSize )
        {
            int bytesSent = send( _socket, ( dataPtr + totalSent ), ( dataSize - totalSent ), 0 );
            if( bytesSent == -1 )
                break;
            totalSent += bytesSent;
        }
    }

public:
    Client() = delete;

    Client( const std::string& IP, const int& PORT )
    {
        _socket = socket( AF_INET, SOCK_STREAM, 0 );
        if( _socket < 0 )
            throw ServerException( ( "listener = " + std::to_string(_socket) ) );

        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(PORT);
        _addr.sin_addr.s_addr = inet_addr(IP.c_str());

        if( connect( _socket, ( struct sockaddr* )&_addr, sizeof(_addr) ) < 0 )
            throw ServerException( "connect" );
        std::cout << _socket << ") " << IP << ':' << PORT << std::endl;
    }

    Client( const Client& rhs )
    {
        this->_socket = rhs._socket;
        this->_addr = rhs._addr;
    }

    Client& operator=( const Client& rhs )
    {
        if( rhs._socket != this->_socket )
        {
            this->_socket = rhs._socket;
            this->_addr = rhs._addr;
        }
        return *this;
    }

    ~Client() {}

    void _close()
    {
        close(_socket);
    }

    void run()
    {
        int r = _socket;
        for( size_t i = 0; i < 10; ++i )
        {
            sendToSock( ( std::to_string(r) + ";" ) );
            std::string msg = readFromSock(); 
            if(  !msg.empty() )
            {
                thread_cout(
                    std::cout << msg << std::endl
                );
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
        }
    }

};

int main()
{
    const size_t I = 100;
    std::vector<Client> clients;
    for( size_t i = 0; i < I; ++i )
        clients.emplace_back( "127.0.0.1", 8000 );

    std::vector<std::thread> threads;
    for( size_t i = 0; i < I; ++i )
        threads.emplace_back( &Client::run, clients[i] );

    for( auto& it : threads )
    {   
        if( it.joinable() )
            it.join();
    }

    for( auto& it : clients )
        it._close();
    return 0;
}


