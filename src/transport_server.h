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
#include "io_context_pool.hpp"
#include "logger.h"
#include "web_utility.h"

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace boost::beast;
using namespace std;
using boost::asio::ip::tcp;

typedef std::shared_ptr<tcp::socket> SocketPtr;

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

//文件上传格式 post http://xxx.com/{dir}/filename.jpg 必须有 Content-Lenght
//文件下载 get http://xxx.com/{dir}/filename.jpg

//文件上传格式 post http://xxx.com/{dir}/filename88766_12398776.mp4 必须有 Content-Lenght
//文件下载 get http://xxx.com/{dir}/filename88766_12398776.mp4

//不支持断点续传,主要用于边上传边下载这种模式，上传的文件会定期清理，文件必须带有扩展名
//如果要支持断点续传,下载方与上传方协商,比如重新指定一个上传文件名进行断点续传

//struct UploadTransportContext
//{
//    CaseInsensitiveMultimap query_params; //url
//    std::list<string> recv_buffers;
//    int size = 0;
//};

//enum SendType
//{
//    ST_STOP_SUCCESS = 0,
//    ST_STOP_UPLOAD_ERROR = 1,
//    ST_SEND_NORMAL = 2,
//};

//struct SendContent
//{
//    SendType st = ST_SEND_NORMAL;
//    string buf;
//    SendContent()
//    {
//    }
//    SendContent(SendContent&& sc)
//    {
//        st = sc.st;
//        buf = std::move(sc.buf);
//    }

//    SendContent& operator = (SendContent&& sc)
//    {
//        st = sc.st;
//        buf = std::move(sc.buf);
//        return *this;
//    }
//};

//struct DownTransportContext
//{
//    boost::smatch path_params;
//    CaseInsensitiveMultimap query_params;
//    std::list<SendContent> send_buffers;
//};

class UploadTask;
typedef std::shared_ptr<UploadTask> UploadTaskPtr;

struct TransportContext
{
    boost::smatch path_params;
    CaseInsensitiveMultimap query_params;
    string file_dir;
    string file_path;
    SocketPtr socket;
    int file_size = 0;
};

typedef std::shared_ptr<TransportContext> TransportContextPtr;

//typedef std::shared_ptr<UploadTransportContext> UploadTransportContextPtr;
//typedef std::shared_ptr<DownTransportContext> DownTransportContextPtr;

class FileTransportServer
{
public:
    FileTransportServer(string listen_address, int listen_port, const string& root_dir);
    ~FileTransportServer();

    void start();

private:
    bool parseTarget(const boost::beast::string_view& target, std::string &path, std::string &query_string);
    CaseInsensitiveMultimap parseQueryString(const std::string &query_string);

    /*****************************************************************************
    *   fiber function per server connection
    *****************************************************************************/
    void session(SocketPtr socket);

    void accept();

private:
    void removeDown(const string& key, const DownTransportContextPtr &down);

private:
    IoContextPool & m_pool;
    tcp::acceptor m_accept;
    string  m_doc_root;

    //map<string, UploadTransportContextPtr> m_upload_transport;
    //map<string, vector<DownTransportContextPtr>> m_down_transport;
    //key为 file_path;
    map<string, UploadTaskPtr> m_upload_tasks;
    boost::fibers::mutex m_mutex;

    boost::regex m_target_file_regex = boost::regex("^/([0-9a-zA-Z]{1,32})/([_0-9a-zA-Z]{1,32}.*)$");
    int m_body_limit = 1024*1024*100;
};

#endif
