#include "kconfig.h"
#include "transport_server.h"

int main(int argc, char **argv)
{
    namespace po = boost::program_options;
    try
    {
        IoContextPool::m_pool_size = 8;
        IoContextPool& pool = IoContextPool::get_instance();

        ConfigParams *params = ConfigParams::GetInstance();

        //初始化
        if (!params->initJsonSetting(argc, argv))
        {
            return -1;
        }
        init_logging(params->m_setting.at("log_dir").get<string>() + "file_transport_server.log",
                     static_cast<boost::log::trivial::severity_level>(params->m_setting.at("log_level").get<int>()));

        string listen_address = "0.0.0.0";
        int listen_port = params->m_setting.at("port").get<int>();
        string root_dir = params->m_setting.at("root_dir").get<string>();
        FileTransportServer tserver(listen_address, listen_port, root_dir);
        cout << "FileTransportServer::GetInstance()->start()\n";
        tserver.start();
        pool.run();
    }
    catch (std::exception const &e)
    {
        cerr << "transport server exit! unhandled exception: " << e.what() << "," << typeid(e).name() << endl;
    }
    return 0;
}
