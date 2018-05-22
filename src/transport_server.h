//          Copyright 2003-2013 Christopher M. Kohlhoff
//          Copyright Oliver Kowalke, Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef TRANSPORT_SERVER_H
#define TRANSPORT_SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <boost/regex.hpp>

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <cstdio>
#include <cctype>
#include <utility>
#include <string>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <functional>
#include <boost/thread.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <boost/fiber/all.hpp>
#include "yield.hpp"
#include "io_service_pool.hpp"
#include "logger.h"
#include "web_utility.h"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace boost::beast;
using namespace std;
using boost::asio::ip::tcp;

typedef std::shared_ptr<tcp::socket> socket_ptr;
typedef http::request<http::string_body> Request;
typedef http::response<http::string_body> Response;


// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
    Stream& stream_;
    bool& close_;
    boost::system::error_code& ec_;

    explicit
    send_lambda(
            Stream& stream,
            bool& close,
            boost::system::error_code& ec)
        : stream_(stream)
        , close_(close)
        , ec_(ec)
    {
    }

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
        // Determine if we should close the connection after
        close_ = ! msg.keep_alive();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::async_write(stream_, sr, boost::fibers::asio::yield[ec_]);
        if(ec_)
        {
            LogErrorExt << ec_.message();
        }
    }
};

//文件上传格式 post http://xxx.com/filename.jpg 必须带有content-length字段
//文件下载 get http://xxx.com/filename.jpg
//不支持断点续传,主要用于边上传边下载这种模式，上传的文件会定期清理

struct UploadTransportContext
{
    CaseInsensitiveMultimap query_params; //url
    std::list<string> recv_buffers;
    int size = 0;
};

enum SendType
{
    ST_STOP_SUCCESS = 0,
    ST_STOP_UPLOAD_ERROR = 1,
    ST_SEND_NORMAL = 2,
};

struct SendContent
{
    SendType st = ST_SEND_NORMAL;
    string buf;
    SendContent()
    {
    }
    SendContent(SendContent&& sc)
    {
        st = sc.st;
        buf = std::move(sc.buf);
    }

    SendContent& operator = (SendContent&& sc)
    {
        st = sc.st;
        buf = std::move(sc.buf);
        return *this;
    }
};

struct DownTransportContext
{
    boost::smatch path_params;
    CaseInsensitiveMultimap query_params;
    std::list<SendContent> send_buffers;
};

typedef std::shared_ptr<UploadTransportContext> UploadTransportContextPtr;
typedef std::shared_ptr<DownTransportContext> DownTransportContextPtr;

class TransportServer
{
public:
    TransportServer(string listen_address, int listen_port, const string& root_dir);
    ~TransportServer();

    void start();

private:
    bool parseTarget(const boost::beast::string_view& target, std::string &path, std::string &query_string);
    CaseInsensitiveMultimap parseQueryString(const std::string &query_string);

    /*****************************************************************************
    *   fiber function per server connection
    *****************************************************************************/
    void session(socket_ptr socket);

    void accept();

private:
    void removeDown(const string& key, const DownTransportContextPtr &down);

private:
    io_service_pool& m_pool;
    tcp::acceptor m_accept;
    string  m_doc_root;

    map<string, UploadTransportContextPtr> m_upload_transport;
    map<string, vector<DownTransportContextPtr>> m_down_transport;

    boost::regex m_target_file_regex = boost::regex("^/([0-9a-z]{8}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{4}-[0-9a-z]{12}.*)$");
    int m_body_limit = 1024*1024*100;
};

#endif
