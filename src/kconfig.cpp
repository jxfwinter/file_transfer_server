#include "kconfig.h"

#include <boost/log/attributes.hpp>
#include <iostream>
using std::cout;
using std::endl;

ConfigParams* g_cfg;

namespace {
std::map<string, boost::log::trivial::severity_level> logger_levels = {
    {"trace", boost::log::trivial::trace},
    {"debug", boost::log::trivial::debug},
    {"info", boost::log::trivial::info},
    {"warning", boost::log::trivial::warning},
    {"error", boost::log::trivial::error},
    {"fatal", boost::log::trivial::fatal}
};
}


bool init_params(int argc, char **argv, ConfigParams& params)
{
    namespace po = boost::program_options;
    try
    {
        string config_file;
        //命令行选项
        po::options_description cmdline_options("Generic options");
        cmdline_options.add_options()
                ("help,h", "produce help message")
                ("config,c", po::value<string>(&config_file)->default_value("../config/file_transport_server.cfg"));

        po::options_description config_file_options("configure file options");
        config_file_options.add_options()
                ("thread_pool", po::value<uint16_t>(), "thread count")

                ("http_listen_addr", po::value<string>(), "http listen address")
                ("http_listen_port", po::value<uint16_t>(), "http listen port")
                ("http_target_prefix", po::value<string>(), "http upload target prefix")

                ("body_limit", po::value<int>(), "body buffer max size limit")
                ("body_duration", po::value<int>(), "body buffer duration seconds")

                ("log_path", po::value<string>(), "log file path")
                ("log_level", po::value<string>(), "log level:trace debug info warning error fatal");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        notify(vm);
        if (vm.count("help"))
        {
            cout << cmdline_options << endl;
            return false;
        }

        std::ifstream ifs(config_file);
        if (!ifs)
        {
            cout << "can not open config file: " << config_file << "\n";
            return false;
        }
        else
        {
            store(po::parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        }

        params.thread_pool = vm["thread_pool"].as<uint16_t>();

        params.http_listen_addr = vm["http_listen_addr"].as<string>();
        params.http_listen_port = vm["http_listen_port"].as<uint16_t>();
        params.http_target_prefix = vm["http_target_prefix"].as<string>();

        params.body_limit = vm["body_limit"].as<int>();
        params.body_duration = vm["body_duration"].as<int>();

        params.log_path = vm["log_path"].as<string>();
        string str_level = vm["log_level"].as<string>();
        auto it = logger_levels.find(str_level);
        if(it != logger_levels.end())
        {
            params.log_level = it->second;
        }
        return true;
    }
    catch (std::exception &e)
    {
        cout << "exception type:" << typeid(e).name() << ",error message:" <<  e.what() << endl;
        return false;
    }
}
