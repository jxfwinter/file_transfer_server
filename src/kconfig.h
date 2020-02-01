#ifndef KCONFIG_H
#define KCONFIG_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <iostream>
#include <set>
#include <chrono>
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/beast.hpp>

#include "logger.h"
#include "web_utility.hpp"

using std::string;
using std::vector;
using std::map;

namespace http = boost::beast::http;
namespace asio = boost::asio;

typedef boost::asio::io_context IoContext;
typedef boost::asio::ip::tcp::acceptor Acceptor;
typedef boost::asio::ip::tcp::endpoint Endpoint;
typedef boost::asio::ip::tcp::socket TcpSocket;
typedef boost::asio::executor_work_guard<boost::asio::io_context::executor_type> IoContextWork;

typedef boost::system::error_code BSError;

typedef http::request<http::string_body> StrRequest;
typedef http::response<http::string_body> StrResponse;

struct ConfigParams
{
    uint16_t thread_pool = 1;

    string http_listen_addr = "0.0.0.0";
    uint16_t http_listen_port = 2080;

    string http_target_prefix;

    int body_limit = 0;
    int body_duration;

    string log_path;
    boost::log::trivial::severity_level log_level = boost::log::trivial::debug;
};

//初始化参数
bool init_params(int argc, char** argv, ConfigParams& params);

//全局变量
extern ConfigParams* g_cfg;
#endif
