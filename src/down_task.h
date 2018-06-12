#ifndef DOWN_TASK_H
#define DOWN_TASK_H

#include "kconfig.h"
#include "transport_server.h"
//#include <boost/thread/sync_queue.hpp>
#include "fiber_unbounded_buffer.h"

class DownTask : public std::enable_shared_from_this<DownTask>
{
public:
    DownTask(const TransportContext& cxt);
    virtual ~DownTask() = default;

    void start();
    void stop();
    void send(const std::shared_ptr<string>& buf);

private:
    std::atomic_bool m_running;
    TransportContext m_cxt;
    boost::fibers::fiber m_send_fiber;
    //boost::fibers::mutex m_mutex;
    //std::queue<std::shared_ptr<string>> m_send_buffers; //后续替换成使用 boost::fibers::condition_variable 的队列
    fiber_unbounded_queue<std::shared_ptr<string>> m_send_buffers;
};

#endif // DOWN_TASK_H
