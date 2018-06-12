#ifndef KCONFIG_H
#define KCONFIG_H

#define BOOST_COROUTINES_NO_DEPRECATION_WARNING

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <iostream>
#include <set>
#include <chrono>
#include <boost/uuid/uuid.hpp>
#include <boost/date_time.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/gregorian/greg_duration.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/fiber/all.hpp>

#include "json.hpp"
#include "exception_trace.h"
#include "logger.h"

using namespace nlohmann;
using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::uuids;
using namespace boost::property_tree;
namespace fs = boost::filesystem;

class ConfigParams
{
  public:
    bool initJsonSetting(int argc, char **argv);
    static ConfigParams *GetInstance();
    json m_setting;

  private:
    ConfigParams();
    static ConfigParams *m_instance;
};

//将字符串时间(不包含日期)转为秒数
int timeStringConvertSeconds(const string &t);

//从url中解析出 host_port与target  example http://sh.xxx.com:5001/push  host_port sh.xxx.com:5001 target /push
//  http://sh.xxx.com/push  host_port sh.xxx.com path /push
bool parseHostPortByHttpUrl(const string &url, string &host_port, string &target);

//从url中解析出 host, port target  example http://sh.xxx.com:5001/push  host sh.xxx.com port 5001 target /push
//  http://sh.xxx.com/push  host port sh.xxx.com target /push
bool parseHostPortByHttpUrl(const string &url, string &host, string& port, string &target);

#endif
