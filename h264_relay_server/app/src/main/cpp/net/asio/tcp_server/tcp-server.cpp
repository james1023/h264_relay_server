
#include "base/debug-msg-c.h"
#include "tcp-server.h"

namespace net {

TcpServer::TcpServer()
 :  enable_unittest_(false)
{
}

TcpServer::~TcpServer()
{
}

void TcpServer::ServerThread(std::string port, std::function<OnStreamServerMsg> on_msg, const void *usr_arg)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::ServerThread: in. \n", __LINE__);

    server_->run();

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::ServerThread: out. \n", __LINE__);
}

bool TcpServer::CreateServer(std::string port, std::function<OnStreamServerMsg> on_msg, const void *usr_arg)
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::CreateServer: in, port=%s. \n", __LINE__, port.c_str());

    std::lock_guard<std::mutex> lock(interface_lock_);
    
    Release();
    
    server_.reset(new net::tcp::server::server("", port, "", 4, on_msg, usr_arg));

    if (nullptr == server_thread_)
        server_thread_.reset(new std::thread(std::bind(&TcpServer::ServerThread, this, port, on_msg, usr_arg)));

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::CreateServer: out. \n", __LINE__);

    return true;
}
    
void TcpServer::Release()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::Release: 0. \n", __LINE__);
    
    if (server_) {
        server_->stop();
        server_.reset();
    }
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::Release: 1. \n", __LINE__);
    
    if (server_thread_) {
        server_thread_->join();
        server_thread_.reset();
    }
    
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::Release: out. \n", __LINE__);
}

void TcpServer::ReleaseServer()
{
    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::ReleaseServer: in. \n", __LINE__);

	std::lock_guard<std::mutex> lock(interface_lock_);

    if (nullptr == server_) {
        DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "? (%u) TcpServer::ReleaseServer: error. \n", __LINE__);
        return;
    }

    Release();

    DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "(%u) TcpServer::ReleaseServer: out. \n", __LINE__);
}

void TcpServer::PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len)
{
    if (nullptr == server_) {
        //DEBUG_TRACE(JAM_DEBUG_LEVEL_DEBUG, "? (%u) TcpServer::PushFrame: error. \n", __LINE__);
        return;
    }

    // 20180329 james: unit test for frame_rate
    //static jam::CheckStreamRate check_stream_rate;
    //check_stream_rate.PrintStreamRate("TcpServer::PushFrame", frame_len);

    if (server_) {
        server_->PushFrame(fmt, frame_buff, frame_len);
    }
}
    
} // namespace net


int CreateServer(const char *port, LiveCbs cbs, const void *usr_arg)
{
	std::string p = port;
    return (int)net::TcpServer::Instance().CreateServer(p, cbs.on_msg, usr_arg);
}

void ReleaseServer()
{
    net::TcpServer::Instance().ReleaseServer();
}

void PushFrame(MEDIA_FRAME_FMT fmt, const unsigned char *frame_buff, int frame_len)
{
    net::TcpServer::Instance().PushFrame(fmt, frame_buff, frame_len);
}