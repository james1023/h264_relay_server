cmake_minimum_required(VERSION 3.4.1)

project(h264_relay_server)

find_library(log-lib log)
set(COMMON_LIBRARY_FILE
	android
	${log-lib})

set(ASIO_INCLUDE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/asio-1.13.0/include)
set(ASIO_DEFINE -DASIO_STANDALONE)

set(TARGET_LIB h264_relay_server)
set(LIB_SRC_FILE
	${CMAKE_CURRENT_SOURCE_DIR}/base/debug-msg.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/debug-msg-c.h
	${CMAKE_CURRENT_SOURCE_DIR}/base/loop.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/base/loop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/net/check_stream.h
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/live_stream_server-c.h
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/tcp-server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/tcp-server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/connection.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/connection_manager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/connection_manager.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/header.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/mime_types.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/mime_types.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/reply.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/reply.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/request.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/request_handler.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/request_handler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/request_parser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/request_parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/net/asio/tcp_server/server_push_frame_mock.h
	${CMAKE_CURRENT_SOURCE_DIR}/native-lib.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/native-lib.h)

add_library(
	${TARGET_LIB}
	SHARED
	${LIB_SRC_FILE})

target_include_directories(
	${TARGET_LIB}
	PRIVATE
	${ASIO_INCLUDE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(
    ${TARGET_LIB}
    PRIVATE
	${DEFINE_FLAGS}
	${ASIO_DEFINE})

target_compile_options(
    ${TARGET_LIB}
    PRIVATE
    ${CXX_FLAGS})

target_link_libraries(
	${TARGET_LIB}
	${COMMON_LIBRARY_FILE})