#include "kconfig.h"

ConfigParams *ConfigParams::m_instance = nullptr;

ConfigParams *ConfigParams::GetInstance()
{
    if (m_instance)
        return m_instance;

    m_instance = new ConfigParams();
    return m_instance;
}

ConfigParams::ConfigParams()
{

}

bool ConfigParams::initJsonSetting(int argc, char **argv)
{
    namespace po = boost::program_options;
    try
    {
        string config_file;
        //命令行选项
        po::options_description cmdline_options("Generic options");
        cmdline_options.add_options()("help,h", "produce help message")("config,c", po::value<string>(&config_file)->default_value("../etc/api_service.json"),
                                                                        "name of a file of a json configuration.");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        notify(vm);
        if (vm.count("help"))
        {
            cout << cmdline_options << endl;
        }

        std::ifstream ifs(config_file);
        if (!ifs)
        {
            cout << "can not open config file: " << config_file << "\n";
            return false;
        }
        else
        {
            ifs >> m_setting;
        }
        return true;
    }
    catch (std::exception &e)
    {
        cout << e.what() << endl;
        return false;
    }
}

int timeStringConvertSeconds(const string &t)
{
    vector<string> r;
    boost::split(r, t, boost::is_any_of(":"));
    if (r.size() != 3)
    {
        throw_with_trace(std::runtime_error("timeStringConvertSeconds failed, t:" + t));
    }
    int hour = boost::lexical_cast<int>(r[0]);
    int minute = boost::lexical_cast<int>(r[1]);
    int seconds = boost::lexical_cast<int>(r[2]);
    return hour * 3600 + minute * 60 + seconds;
}

bool parseHostPortByHttpUrl(const string &url, string &host_port, string &target)
{
    string tmp = url.substr(7); //http://
    size_t host_port_end = tmp.find('/');
    if (host_port_end == std::string::npos)
    {
        host_port = tmp;
        target = "/";
        return true;
    }
    host_port = tmp.substr(0, host_port_end);
    target = tmp.substr(host_port_end);
    return true;
}

bool parseHostPortByHttpUrl(const string &url, string &host, string& port, string &target)
{
    string host_port;
    if(!parseHostPortByHttpUrl(url, host_port, target))
    {
        return false;
    }
    size_t host_end = host_port.find(':');
    if(host_end == std::string::npos)
    {
        host = host_port;
        port = "80";
        return true;
    }
    host = host_port.substr(0, host_end);
    port = host_port.substr(host_end+1);
    return true;
}
