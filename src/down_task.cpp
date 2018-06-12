#include "down_task.h"

DownTask::DownTask(const TransportContext& cxt) : m_cxt(cxt)
{
    m_running = false;
}

void DownTask::start()
{
    m_running = true;
    auto self(shared_from_this());
    m_send_fiber = boost::fibers::fiber([this, self]() {
        auto socket = m_cxt.socket;
        boost::beast::error_code ec;

        http::response<http::buffer_body> res;
        res.result(http::status::ok);
        res.version(11);
        res.content_length(m_cxt.file_size);
        res.body().data = nullptr;
        res.body().more = true;
        http::response_serializer<http::buffer_body, http::fields> sr{res};
        http::async_write_header(*socket, sr, boost::fibers::asio::yield[ec]);
        if(ec)
        {
            LogErrorExt << ec.message();
            m_running = false;
            return;
        }
        std::shared_ptr<string> buf;
        while(1)
        {
//            {
//                std::unique_lock<boost::fibers::mutex> lk(m_mutex);
//                if(m_send_buffers.empty())
//                {
//                    lk.unlock();
//                    boost::this_fiber::sleep_for(std::chrono::milliseconds(10));
//                    continue;
//                }
//                else
//                {
//                    buf = m_send_buffers.front();
//                    m_send_buffers.pop();
//                }
//            }
            buf = m_send_buffers.pop();
            if(!buf) //空指针 结束
            {
                res.body().data = nullptr;
                res.body().more = false;
                http::async_write(*socket, sr, boost::fibers::asio::yield[ec]);
                if(ec == http::error::need_buffer)
                {
                    ec = {};
                }
                if(ec)
                {
                    LogErrorExt << ec.message();
                    m_running = false;
                    return;
                }
                m_running = false;
            }

            res.body().data = const_cast<char*>(buf->c_str());
            res.body().size = buf->size();
            res.body().more = true;
            http::async_write(*socket, sr, boost::fibers::asio::yield[ec]);
            if(ec == http::error::need_buffer)
            {
                ec = {};
            }
            if(ec)
            {
                LogErrorExt << ec.message();
                m_running = false;
                return;
            }


        }
    });
}

void DownTask::stop()
{
    {
        //发送空指针表示结束
        //std::lock_guard<boost::fibers::mutex> lk(m_mutex);
        m_send_buffers.push(nullptr);
    }
    if(m_send_fiber.joinable())
    {
        m_send_fiber.join();
    }
    m_running = false;
}

void DownTask::send(const std::shared_ptr<string>& buf)
{
    if(!m_running)
        return;
    //std::lock_guard<boost::fibers::mutex> lk(m_mutex);
    m_send_buffers.push(buf);
}
