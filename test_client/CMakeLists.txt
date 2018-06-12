project(transport_client)
cmake_minimum_required(VERSION 2.8)
set(OUTDIR ../bin)
#set(Boost_USE_STATIC_LIBS ON)
#set(Boost_USE_MULTITHREADED ON)
#set(CMAKE_BUILD_TYPE DEBUG)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

set(Boost_LIBRARIES boost_log boost_fiber boost_context boost_log_setup boost_thread boost_filesystem boost_system boost_program_options boost_regex boost_date_time boost_chrono boost_locale)
include_directories(../server_kit/include  ../server_kit/include/web_tool)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../server_kit/lib)
set(KLink_LIBRARIES web_tool soci_core soci_postgresql nghttp2_asio nghttp2 backtrace pthread pq dl crypto ssl)
SET(EXECUTABLE_OUTPUT_PATH ${OUTDIR})
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
MESSAGE(STATUS "cxx flags is ${CMAKE_CXX_FLAGS}")

aux_source_directory(./ SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} -Wl,--start-group ${Boost_LIBRARIES} ${KLink_LIBRARIES} -Wl,--end-group)






