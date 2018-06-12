//          Copyright 2003-2013 Christopher M. Kohlhoff
//          Copyright Oliver Kowalke, Nat Goodspeed 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "transport_server.h"
#include "down_task.h"
#include "upload_task.h"

typedef std::shared_ptr<DownTask> DownTaskPtr;

boost::beast::string_view mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/octet-stream";
}

std::string path_cat(boost::beast::string_view base, boost::beast::string_view path)
{
    if(base.empty())
        return path.to_string();
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

FileTransportServer::FileTransportServer(string listen_address, int listen_port, const string& root_dir) :
    m_pool(IoContextPool::get_instance()),
    m_accept(m_pool.get_io_context(), tcp::endpoint(boost::asio::ip::address::from_string(listen_address), listen_port)),
    m_doc_root(root_dir)
{
}


FileTransportServer::~FileTransportServer()
{

}

void FileTransportServer::session(SocketPtr socket)
{
    bool close = false;
    boost::system::error_code ec;

    // This buffer is required to persist across reads
    boost::beast::multi_buffer buffer;

    // This lambda is used to send messages
    send_lambda<tcp::socket> send{*socket, close, ec};

    try
    {
        for(;;)
        {
            // Read a request
            http::request_parser<http::buffer_body> p;
            p.body_limit(m_body_limit);
            http::async_read_header(*socket, buffer, p, boost::fibers::asio::yield[ec]);
            if(ec)
            {
                LogErrorExt << ec.message();
                return;
            }
            auto& req = p.get();

            // Returns a bad request response
            auto const bad_request =
                    [&req](boost::beast::string_view why)
            {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = why.to_string();
                res.prepare_payload();
                return res;
            };

            // Returns a not found response
            auto const not_found =
                    [&req](boost::beast::string_view target)
            {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = "The resource '" + target.to_string() + "' was not found.";
                res.prepare_payload();
                return res;
            };

            // Request path must be absolute and not contain "..".
            if( req.target().empty() ||
                    req.target()[0] != '/' ||
                    req.target().find("..") != boost::beast::string_view::npos)
                return send(bad_request("Illegal request-target"));

            string path;
            string query_string;
            if(!parseTarget(req.target(), path, query_string))
            {
                LogErrorExt << "parse target error,target:" << req.target().data();
                return send(bad_request("Parse target failed"));
            }

            boost::smatch sm_res;
            if(!boost::regex_match(path, sm_res, m_target_file_regex))
            {
                LogErrorExt << "regex_match error,target:" << req.target().data();
                return send(bad_request("Illegal request-target"));
            }
            TransportContext cxt;
            cxt.socket = socket;
            cxt.query_params = parseQueryString(query_string);
            cxt.path_params = sm_res;
            cxt.file_dir = m_doc_root + sm_res[1];
            cxt.file_path = cxt.file_dir + "/" + sm_res[2];
            if(req.method() == http::verb::get)
            {
                LogDebug <<"get," << req.target();
                boost::beast::error_code ec;
                http::file_body::value_type body;
                body.open(cxt.file_path.c_str(), boost::beast::file_mode::scan, ec);
                //本地文件不存在
                if(ec == boost::system::errc::no_such_file_or_directory)
                {
                    UploadTaskPtr upload_task;
                    {
                        std::lock_guard<boost::fibers::mutex> lk(m_mutex);
                        auto it = m_upload_tasks.find(cxt.file_path);
                        if(it != m_upload_tasks.end())
                        {
                            upload_task = it->second;
                        }
                    }
                    if(!upload_task)
                        return send(not_found(req.target()));

                    cxt.file_size = upload_task->getFileSize();
                    DownTaskPtr down_task = std::make_shared<DownTask>(cxt);
                    upload_task->addDownTask(down_task);
                }
                else
                {
                    //本地文件存在
                    http::response<http::file_body> res
                    {
                        std::piecewise_construct,
                                std::make_tuple(std::move(body)),
                                std::make_tuple(http::status::ok, req.version())
                    };
                    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                    res.set(http::field::content_type, mime_type(cxt.file_path));
                    res.content_length(body.size());
                    res.keep_alive(req.keep_alive());
                    return send(std::move(res));
                }
            }
            else if(req.method() == http::verb::post)
            {
                auto it_clen = req.find("Content-Length");
                if(it_clen == req.end())
                {
                    LogErrorExt << "not has Content-Length, file:" << cxt.file_path;
                    return;
                }
                cxt.file_size = std::stoi((*it_clen).value().data());
                if(cxt.file_size == 0)
                {
                    LogErrorExt << "file size is 0";
                    return;
                }
                UploadTaskPtr upload_task;
                {
                    std::lock_guard<boost::fibers::mutex> lk(m_mutex);
                    auto it = m_upload_tasks.find(cxt.file_path);
                    if(it != m_upload_tasks.end())
                    {
                        it->second->stop(STOP_REASEON::ERROR);
                        m_upload_tasks.erase(it);
                    }
                    upload_task = std::make_shared<UploadTask>(cxt);
                    m_upload_tasks[cxt.file_path] = upload_task;
                }

                upload_task->start();
                int recv_size = 0;
                while(!p.is_done())
                {
                    char buf[1024];
                    p.get().body().data = buf;
                    p.get().body().size = sizeof(buf);
                    http::async_read(*socket, buffer, p, boost::fibers::asio::yield[ec]);
                    if(ec == http::error::need_buffer)
                    {
                        ec.assign(0, ec.category());
                    }
                    if(ec)
                    {
                        LogErrorExt << ec.message();
                        upload_task->stop(STOP_REASEON::ERROR);

                        std::lock_guard<boost::fibers::mutex> lk(m_mutex);
                        m_upload_tasks.erase(cxt.file_path);
                        return;
                    }
                    recv_size += (sizeof(buf) - p.get().body().size);
                    string recv_buf(buf, sizeof(buf) - p.get().body().size);
                    upload_task->recv(std::move(recv_buf));
                }
                if(recv_size != cxt.file_size)
                {
                    LogErrorExt << "recv size not eq upload-size," << recv_size << "," << cxt.file_size;
                    upload_task->stop(STOP_REASEON::ERROR);

                    {
                        std::lock_guard<boost::fibers::mutex> lk(m_mutex);
                        m_upload_tasks.erase(cxt.file_path);
                    }
                    return send(bad_request("recv size not eq content-length"));
                }
                else
                {
                    LogErrorExt << "recv file success," << cxt.file_path;
                    upload_task->stop(STOP_REASEON::NORMAL);

                    {
                        std::lock_guard<boost::fibers::mutex> lk(m_mutex);
                        m_upload_tasks.erase(cxt.file_path);
                    }
                    http::response<http::empty_body> res{http::status::ok, req.version()};
                    return send(std::move(res));;
                }
            }
            else
            {
                return send(bad_request("not support method"));;
            }
            if(close)
            {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        }

        // Send a TCP shutdown
        socket->shutdown(tcp::socket::shutdown_send, ec);
    }
    catch(std::exception& e)
    {
        LogErrorExt << e.what();
    }
}

void FileTransportServer::accept()
{
    try
    {
        for (;;)
        {
            SocketPtr socket(new tcp::socket(m_pool.get_io_context()));
            boost::system::error_code ec;
            m_accept.async_accept(
                        *socket,
                        boost::fibers::asio::yield[ec]);
            if (ec)
            {
                throw boost::system::system_error(ec); //some other error
            }
            else
            {
                boost::asio::post(socket->get_executor(), [socket, this](){
                    boost::fibers::fiber([socket, this]() {
                        try
                        {
                            this->session(socket);
                        }
                        catch (std::exception const &e)
                        {
                            LogErrorExt << e.what();
                        }
                    }).detach();
                });
            }
        }
    }
    catch (std::exception const &e)
    {
        LogErrorExt << e.what();
    }
    //m_pool->stop();
}

void FileTransportServer::start()
{
    boost::fibers::fiber([this](){
        this->accept();
    }).detach();
}

bool FileTransportServer::parseTarget(const boost::beast::string_view& target, std::string &path, std::string &query_string)
{
    size_t query_start = target.find('?');
    if(query_start != boost::beast::string_view::npos)
    {
        path = target.substr(0, query_start).to_string();
        query_string = target.substr(query_start + 1).to_string();
    }
    else
    {
        path = target.to_string();
    }
    return true;
}

CaseInsensitiveMultimap FileTransportServer::parseQueryString(const std::string &query_string)
{
    CaseInsensitiveMultimap result;

    if(query_string.empty())
        return result;

    size_t name_pos = 0;
    size_t name_end_pos = -1;
    size_t value_pos = -1;
    for(size_t c = 0; c < query_string.size(); ++c) {
        if(query_string[c] == '&') {
            auto name = query_string.substr(name_pos, (name_end_pos == std::string::npos ? c : name_end_pos) - name_pos);
            if(!name.empty()) {
                auto value = value_pos == std::string::npos ? std::string() : query_string.substr(value_pos, c - value_pos);
                result.emplace(std::move(name), Percent::decode(value));
            }
            name_pos = c + 1;
            name_end_pos = -1;
            value_pos = -1;
        }
        else if(query_string[c] == '=') {
            name_end_pos = c;
            value_pos = c + 1;
        }
    }
    if(name_pos < query_string.size()) {
        auto name = query_string.substr(name_pos, name_end_pos - name_pos);
        if(!name.empty()) {
            auto value = value_pos >= query_string.size() ? std::string() : query_string.substr(value_pos);
            result.emplace(std::move(name), Percent::decode(value));
        }
    }

    return result;
}
