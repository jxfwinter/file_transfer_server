#include "logger.h"

#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/core/null_deleter.hpp>

#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>

#include <boost/log/sinks/async_frontend.hpp>

#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

using namespace std;

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

#ifdef KLOGGER_ASYNC
typedef sinks::asynchronous_sink< sinks::text_ostream_backend > StreamSink;
typedef sinks::asynchronous_sink< sinks::text_file_backend > FileSink;
#else
typedef sinks::synchronous_sink<sinks::text_ostream_backend> StreamSink;
typedef sinks::synchronous_sink<sinks::text_file_backend> FileSink;
#endif

//logName示例: /var/log/test.log
void init_logging(const std::string &log_path, logging::trivial::severity_level filter_level)
{
    logging::add_common_attributes();

    boost::filesystem::path path_log_filename(log_path);
    boost::filesystem::path parent_path = path_log_filename.parent_path();
    boost::filesystem::path stem = path_log_filename.stem();
    boost::filesystem::path extension_name = path_log_filename.extension();
    auto core = logging::core::get();

    //这里不使用target_file_name,因为程序重启以前的file_name内容会被清空,这里不想使用日志追加
    boost::shared_ptr<sinks::text_file_backend> file_backend = boost::make_shared<sinks::text_file_backend>(
                keywords::file_name = parent_path.string() + "/" + stem.string() + "_%6N" + extension_name.string(),
                //keywords::target_file_name = parent_path.string() + "/" + stem.string() + "_%6N" + extension_name.string(),
                keywords::rotation_size = 50 * 1024 * 1024
            //keywords::time_based_rotation = sinks::file::rotation_at_time_point(0,0,0)
            );
#ifndef KLOGGER_FORBIDDEN_AUTO_FLUSH
    //写入日志文件的默认使用 auto_flush，防止日志丢失, false会缓冲
    file_backend->auto_flush(true);
#else
    file_backend->auto_flush(true);
#endif

    boost::shared_ptr<FileSink> file_sink = boost::make_shared<FileSink>(file_backend);
    file_sink->locked_backend()->set_file_collector(
                sinks::file::make_collector(
                    keywords::target = parent_path.string(),
                    keywords::max_size = (uintmax_t)5 * 1024 * 1024 * 1024,
                    keywords::min_free_space = 100 * 1024 * 1024));
    core->add_sink(file_sink);

    file_sink->set_filter(expr::attr<logging::trivial::severity_level>("Severity") >= filter_level);
    file_sink->set_formatter(
                expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << "]"
                << "[" << expr::attr<logging::trivial::severity_level>("Severity") << "]"
                << "[" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]"
                << " " << expr::max_size_decor(1024*50, ">>>")[expr::stream << expr::message]
                );
    file_sink->locked_backend()->scan_for_files();

    //控制台输出
    auto console_backend = boost::make_shared<sinks::text_ostream_backend>();
    console_backend->add_stream(
                boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

#ifndef KLOGGER_FORBIDDEN_AUTO_FLUSH
    //指定立刻将日志打印到屏幕, false会缓冲日志，直到合适的时候再打印到屏幕，防止日志量太大时 io 压力过大
    console_backend->auto_flush(true);
#else
    console_backend->auto_flush(false);
#endif

    boost::shared_ptr<StreamSink> console_sink = boost::make_shared<StreamSink>(console_backend);
    core->add_sink(console_sink);

    console_sink->set_filter(expr::attr<logging::trivial::severity_level>("Severity") >= filter_level);
    console_sink->set_formatter(
                expr::stream
                << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << "]"
                << "[" << expr::attr<logging::trivial::severity_level>("Severity") << "]"
                << "[" << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "]"
                << " " << expr::max_size_decor(1024*50, ">>>")[expr::stream << expr::message]
                );
}

