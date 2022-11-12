cmake_minimum_required(VERSION 3.4.1)

# [MSVC] Select Platform Toolset
# cmake .. -T v120
# or
# set(CMAKE_GENERATOR_TOOLSET "v120" CACHE STRING "Platform Toolset" FORCE)

# MSVC auto build
#cmake --build . --target ALL_BUILD --config Debug

project(live_client)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_VERBOSE_MAKEFILE TRUE)
#set(CMAKE_CXX_STANDARD 11)

find_library(log-lib log)
set(COMMON_LIBRARY_FILE
		android
		${log-lib})

set(ASIO_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/asio-1.13.0/include)
set(ASIO_DEFINE -DASIO_STANDALONE)

set(TARGET_LIB h264_client)
set(LIB_SRC_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/base/debug-msg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base/debug-msg-c.h
	${CMAKE_CURRENT_SOURCE_DIR}/base/loop.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/loop.h
	${CMAKE_CURRENT_SOURCE_DIR}/base/url/url.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/url/url.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/url/url_own.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/url/url_own.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/check_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/event/event_proc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/IBaseStream.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/CTCPStream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/CTCPStream.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream_live.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream_live.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream_manager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream_manager.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream_live.pb.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_client/stream-client-i.h
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/bs.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/bs.h
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/h264_sei.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/h264_sei.h
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/h264_stream.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/h264_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/H264Rtp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/H264Rtp.h
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/parser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/media/h264/parser.h
	${CMAKE_CURRENT_SOURCE_DIR}/native-lib.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/native-lib.h)

add_library(
	${TARGET_LIB}
	SHARED
	${LIB_SRC_FILE})

target_include_directories(
    ${TARGET_LIB}
    PUBLIC
    ${ASIO_INCLUDE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(
    ${TARGET_LIB}
    PUBLIC
	${DEFINE_FLAGS}
    ${ASIO_DEFINE})

target_compile_options(
    ${TARGET_LIB}
    PUBLIC
    ${CXX_FLAGS})

target_link_libraries(
	${TARGET_LIB}
	${COMMON_LIBRARY_FILE})