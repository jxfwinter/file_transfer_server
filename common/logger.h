#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <fstream>
#include <list>
#include <locale>
#include <string>
#include <ctime>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

/*
宏定义说明
KLOGGER_FORBIDDEN_AUTO_FLUSH 表示每行日志立即刷新到文件或终端
KLOGGER_ASYNC  表示启用异步日志
*/

#define LogTrace	BOOST_LOG_TRIVIAL(trace)
#define LogDebug	BOOST_LOG_TRIVIAL(debug)
#define LogInfo		BOOST_LOG_TRIVIAL(info)
#define LogWarn		BOOST_LOG_TRIVIAL(warning)
#define LogError	BOOST_LOG_TRIVIAL(error)
#define LogFatal	BOOST_LOG_TRIVIAL(fatal)

#define LogDebugExt	LogDebug << __FILE__ << ",line " << __LINE__ << ","
#define LogErrorExt LogError << __FILE__ << ",line " << __LINE__ << ","
#define LogWarnExt LogWarn << __FILE__ << ",line " << __LINE__ << ","
#define LogFatalExt LogFatal << __FILE__ << ",line " << __LINE__ << ","

//注意,异步日志在压力测试时,会因为日志队列导致内存不断增长

//必须先调用
void init_logging(const std::string &log_path, boost::log::trivial::severity_level filter_level);


#endif
