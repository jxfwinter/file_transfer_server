#include "upload_task.h"
#include "down_task.h"

UploadTask::UploadTask(const TransportContext& cxt) : m_cxt(cxt)
{
    m_tmp_filepath = cxt.file_path + ".tmp";
}

void UploadTask::start()
{
    boost::system::error_code e;
    fs::path tmp_path(m_cxt.file_path);
    fs::remove(tmp_path, e);
    tmp_path = m_tmp_filepath;
    fs::remove(tmp_path, e);
    tmp_path = m_cxt.file_dir;
    if(!boost::filesystem::is_directory(tmp_path, e))
    {
        boost::filesystem::create_directory(tmp_path, e);
    }
    m_ofs.open(m_tmp_filepath, std::ios_base::binary);
}

void UploadTask::stop(STOP_REASEON r)
{
    m_ofs.close();
    if(r == STOP_REASEON::NORMAL)
    {
        fs::path tmp_path(m_tmp_filepath);
        fs::path new_path(m_cxt.file_path);
        boost::system::error_code e;
        fs::rename(tmp_path, new_path, e);
        if(e)
        {
            LogErrorExt << e.message() << "," << tmp_path << "," << new_path;
        }
    }

    {
        std::lock_guard<boost::fibers::mutex> lk(m_down_mutex);
        for(DownTaskPtr& d : m_down_tasks)
        {
            d->stop();
        }
    }
}

void UploadTask::addDownTask(DownTaskPtr task)
{
    task->start();
    {
        std::lock_guard<boost::fibers::mutex> lk(m_buffers_mutex);
        for(std::shared_ptr<string>& s : m_recv_buffers)
        {
            task->send(s);
        }
    }
    {
        std::lock_guard<boost::fibers::mutex> lk(m_down_mutex);
        m_down_tasks.push_back(task);
    }
}

void UploadTask::recv(string buf)
{
    m_ofs.write(buf.c_str(), buf.size());
    std::shared_ptr<string> pbuf = std::make_shared<string>(std::move(buf));
    {
        std::lock_guard<boost::fibers::mutex> lk(m_buffers_mutex);
        m_recv_buffers.push_back(pbuf);
    }
    {
        std::lock_guard<boost::fibers::mutex> lk(m_down_mutex);
        for(DownTaskPtr& d : m_down_tasks)
        {
            d->send(pbuf);
        }
    }
}
