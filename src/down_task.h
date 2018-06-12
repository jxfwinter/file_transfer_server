#ifndef DOWN_TASK_H
#define DOWN_TASK_H

#include "kconfig.h"
#include "transport_server.h"

class DownTask : public std::enable_shared_from_this<DownTask>
{
public:
    DownTask(const TransportContext& cxt);
    virtual ~DownTask();

    void start();
    void stop();
    void send(const std::shared_ptr<string>& buf);

private:

}

#endif // DOWN_TASK_H
