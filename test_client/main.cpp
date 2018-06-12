//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, coroutine
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <cstdlib>
#include <functional>
#include <fstream>
#include <iostream>
#include <chrono>
#include <string>
#include "json.hpp"
#include <boost/lexical_cast.hpp>
#include <boost/fiber/all.hpp>
#include "web_tool/io_context_pool.hpp"
#include "web_tool/yield.hpp"

using namespace nlohmann;
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
using namespace std;
//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

void do_publish(std::string const& host,
                std::string const& port,
                std::string const& filename,
                io_service_pool& pool)
{
    boost::system::error_code ec;
    string target;
    int body_size = random()% (1024*1024*100);
    target = "/" + filename;

    string seq_chars = "qwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbnmqwertyuiopasdfghjklzxcvbnm";
    //string seq_chars = "qwertyuiopasdfghjklzxcvbnm";

    int nsend = body_size / seq_chars.size(); //需要发送多少次
    int last_send_len = body_size % seq_chars.size(); //最后一次需要发送的字节数

    std::cout << "pub," << host << "," << port << "," << target << "," << body_size << ",per size:" << seq_chars.size() << ",nsend:"
              << nsend << ",last_send:" << last_send_len << std::endl;

    // These objects perform our I/O
    tcp::resolver resolver{pool.get_io_service()};
    tcp::socket socket{pool.get_io_service()};

    // Look up the domain name
    auto const lookup = resolver.async_resolve({host, port}, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "pub resolve");

    // Make the connection on the IP address we get from a lookup
    boost::asio::async_connect(socket, lookup, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "pub connect");

    http::request<http::buffer_body> req{http::verb::post, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.version(11);
    req.content_length(body_size);
    req.body().data = nullptr;
    req.body().more = true;

    http::request_serializer<http::buffer_body, http::fields> sr(req);
    http::async_write_header(socket, sr, boost::fibers::asio::yield[ec]);
    if(ec)
    {
        return fail(ec, "pub write header");
    }

    ofstream ofs("upload_" + filename, std::ios_base::binary);
    int isend = 0;
    while(1)
    {
        req.body().data = const_cast<char*>(seq_chars.c_str());
        req.body().size = seq_chars.size();
        req.body().more = true;
        http::async_write(socket, sr, boost::fibers::asio::yield[ec]);
        if(ec == http::error::need_buffer)
        {
            ec = {};
        }
        if(ec)
        {
            return fail(ec, "pub write");
        }
        ofs.write(seq_chars.c_str(), seq_chars.size());
        ++isend;
        if(isend == nsend)
        {
            break;
        }
        //测试使用,不要很快就发完了
        //boost::this_fiber::sleep_for(std::chrono::microseconds(50));
    }

    //最后一次发送
    if(last_send_len != 0)
    {
        req.body().data =  const_cast<char*>(seq_chars.c_str());
        req.body().size = last_send_len;
        req.body().more = false;
        http::async_write(socket, sr, boost::fibers::asio::yield[ec]);
        if(ec)
        {
            return fail(ec, "pub write");
        }
        ofs.write(seq_chars.c_str(), last_send_len);
        ofs.close();
    }

    // This buffer is used for reading and must be persisted
    boost::beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::string_body> res;

    // Receive the HTTP response
    http::async_read(socket, b, res, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "pub read");
    std::cout << "pub," << res << "\n";
}

void do_subscribe(std::string const& host,
                  std::string const& port,
                  std::string const& filename,
                  io_service_pool& pool)
{

    boost::system::error_code ec;
    string target;
    target = "/" + filename;

    std::cout << "sub," << host << "," << port << "," << target << std::endl;

    // These objects perform our I/O
    tcp::resolver resolver{pool.get_io_service()};
    tcp::socket socket{pool.get_io_service()};

    // Look up the domain name
    auto const lookup = resolver.async_resolve({host, port}, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "sub resolve");

    // Make the connection on the IP address we get from a lookup
    boost::asio::async_connect(socket, lookup, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "sub connect");

    http::request<http::empty_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    http::async_write(socket, req, boost::fibers::asio::yield[ec]);
    if(ec)
    {
        return fail(ec, "sub write header");
    }

    boost::beast::multi_buffer b;
    //http::response<http::buffer_body>
    //http::response<http::dynamic_body> res;
    http::response_parser<http::dynamic_body> p;
    p.body_limit(1024*1024*100);

    // Receive the HTTP response
    http::async_read(socket, b, p, boost::fibers::asio::yield[ec]);
    if(ec)
        return fail(ec, "sub read");
    auto& res = p.get();
    auto& body = res.body();
    std::cout << "sub,result:" << res.result() << ",body size:" << body.size() << "\n";
    if(res.result() != http::status::ok)
    {
        return;
    }

//    string tmp = boost::beast::buffers_to_string(body.data());
//    ofstream ofs;
//    ofs.open("./subscribe_" + filename, std::ios_base::binary);
//    ofs.write(tmp.c_str(), tmp.size());
//    ofs.close();

    const auto& const_data = body.data();
    ofstream ofs;
    ofs.open("./subscribe_" + filename, std::ios_base::binary);
    for(boost::asio::const_buffer buffer : boost::beast::detail::buffers_range(const_data))
    {
        ofs.write(static_cast<const char*>(buffer.data()), buffer.size());
    }
    ofs.close();
}

// Performs an HTTP GET and prints the response
void
do_session(
        std::string const& host,
        std::string const& port,
        std::string const& filename,
        int subscribe_count,
        io_service_pool& pool)
{
    boost::fibers::fiber(&do_publish,
                         host,
                         port,
                         filename,
                         std::ref(pool)).detach();

    //boost::this_fiber::sleep_for(std::chrono::microseconds(50));
    for(int i=0; i<subscribe_count; ++i)
    {
        boost::fibers::fiber(&do_subscribe,
                             host,
                             port,
                             filename,
                             //"fc4ff3c7-bfa4-4c5d-9fe7-440530bb5bd3.mp4",
                             std::ref(pool)).detach();
    }

}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    srand((int)time(0));
    // Check command line arguments.
    if(argc != 4)
    {
        std::cerr <<
                     "Usage: test-client <host> <port> <count>\n" <<
                     "Example:\n" <<
                     "transport_client 127.0.0.1 8921 1 \n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const count =  static_cast<unsigned short>(std::atoi(argv[3]));

    // The io_service is required for all I/O
    //boost::asio::io_service ios;
    io_service_pool::m_pool_size = 3;
    io_service_pool& pool = io_service_pool::GetInstance();

    for(int i=0; i<count; ++i)
        // Launch the asynchronous operation
    {
        string filename = boost::lexical_cast<string>(boost::uuids::random_generator()());
        filename += ".test";
        boost::fibers::fiber(&do_session,
                             std::string(host),
                             std::string(port),
                             std::string(filename),
                             1,
                             std::ref(pool)
                             ).detach();
    }

    // Run the I/O service. The call will return when
    // the get operation is complete.
    pool.run();

    return EXIT_SUCCESS;
}
