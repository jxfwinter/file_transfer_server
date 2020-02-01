#ifndef TRANSPORT_SERVER_H
#define TRANSPORT_SERVER_H

#include "kconfig.h"

//文件上传格式 post http://xxx.com/{dir}/filename.jpg 必须有 Content-Lenght
//文件下载 get http://xxx.com/{dir}/filename.jpg

//文件上传格式 post http://xxx.com/{dir}/filename88766_12398776.mp4 必须有 Content-Lenght
//文件下载 get http://xxx.com/{dir}/filename88766_12398776.mp4

//不支持断点续传,主要用于边上传边下载这种模式，上传的文件会定期清理，文件必须带有扩展名
//如果要支持断点续传,下载方与上传方协商,比如重新指定一个上传文件名进行断点续传

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
    IoContextPool & m_pool;
    tcp::acceptor m_accept;
    string  m_doc_root;

    //key为 file_path;
    map<string, UploadTaskPtr> m_upload_tasks;
    boost::fibers::mutex m_mutex;

    boost::regex m_target_file_regex = boost::regex("^/([0-9a-zA-Z]{1,32})/([_0-9a-zA-Z]{1,32}.*)$");
    int m_body_limit = 1024*1024*100;
};

#endif
