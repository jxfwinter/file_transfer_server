#ifndef UPLOAD_TASK_H
#define UPLOAD_TASK_H
#include "kconfig.h"
#include "transport_server.h"

enum class STOP_REASEON
{
    NORMAL = 0,
    ERROR = 1
};

class DownTask;
typedef std::shared_ptr<DownTask> DownTaskPtr;

class UploadTask : public std::enable_shared_from_this<UploadTask>
{
public:
    UploadTask(const TransportContext& cxt);
    virtual ~UploadTask();
    int getFileSize() {return m_cxt.file_size; }
    void start();
    void stop(STOP_REASEON r);

    void addDownTask(DownTaskPtr task);
    void recv(string buf);

private:
    TransportContext m_cxt;
    string m_tmp_filepath;
    std::vector<std::shared_ptr<string>> m_recv_buffers;
    boost::fibers::mutex m_buffers_mutex;

    boost::fibers::mutex m_down_mutex;
    vector<DownTaskPtr> m_down_tasks;
    ofstream m_ofs;
}

#endif // UPLOADTASK_H
