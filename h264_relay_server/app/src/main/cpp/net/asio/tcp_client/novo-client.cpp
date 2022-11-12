
#include "base/debug-msg-c.h"
#include "net/asio/http/http-client.h"
#include "net/asio/url/url_own.h"
#include "net/asio/stream/check_stream.h"

namespace jam {

HClient::HClient(boost::asio::io_service &io_service,
                 const std::string &host, const std::size_t port, const std::string &path, const std::string &query,
                 const std::string &username, const std::string &password):
socket_(io_service),
host_(host), port_(port), path_(path), query_(query),
has_post_header_(false),
header_connection_(HTTP_HEADER_CONNECTION_CLOSE),
header_method_(HTTP_HEADER_METHOD_NONEED)
{
    
}

HClient::~HClient()
{
	socket_.close();
}

int HClient::RequestData(const unsigned char *data, unsigned int data_len)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::SendData. \n", __LINE__))

	boost::shared_ptr<boost::asio::streambuf> request(new boost::asio::streambuf);

	std::ostream request_stream(request.get());
    std::stringstream ss;

    if (HTTP_HEADER_METHOD_HAS_HEADER == get_header_method()) {
		request_stream << data;

        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host_), port_);
		socket().async_connect(ep,
			boost::bind(&HClient::HandleConnectRequest, shared_from_this(),
				request, boost::asio::placeholders::error));

		return 1;
	}
    else if (HTTP_HEADER_METHOD_NONEED == get_header_method()) {
		if (HTTP_HEADER_CONNECTION_KEEPALIVE == get_header_connection()) {
	
			DEBUG_TRACE(JAM_DEBUG_LEVEL_INFO, "(%u) HClient::RequestData, connect host_=%s, port_=%u. \n", __LINE__, host_.c_str(), port_);

            boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host_), port_);

			// 20170821 james.ch: use connect by synchronously.
#if 1
			try {
				socket().connect(ep);
			}
			catch (std::exception& e) {
				DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "(%u) HClient::RequestData, connect error, host_=%s, port_=%u. \n", __LINE__, host_.c_str(), port_);
				return 0;
			}

			boost::asio::ip::tcp::no_delay option(true);
			socket().set_option(option);

			data_.reset(new std::vector<unsigned char>(data, data + data_len));
			boost::asio::async_write(socket(), boost::asio::buffer(*data_),
				boost::bind(&HClient::HandleKeepWrite, shared_from_this(),
					data_, boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
#else
			socket().async_connect(ep,
				boost::bind(&HClient::HandleConnect, shared_from_this(),
					buff, boost::asio::placeholders::error));
#endif
		}

		return 1;
	}

    switch (get_header_method())
	{
	default:
	case HTTP_HEADER_METHOD_GET: 
        request_stream << "GET " << path_ << "?" << query_ << " HTTP/1.1\r\n";
		break;
	case HTTP_HEADER_METHOD_POST: 
        request_stream << "POST " << path_ << " HTTP/1.1\r\n";
		break;
	case HTTP_HEADER_METHOD_PUT: 
        request_stream << "PUT " << path_ << " HTTP/1.1\r\n";
		break;
	}
    
    request_stream << "Host: " << host_ << ":" << port_ << "\r\n";
    //request_stream << "Accept: text/html,application/xhtml+xml,application/xml,image/webp,*/*\r\n";
	request_stream << "Accept: */*\r\n";
	//request_stream << "Accept-Encoding: gzip, deflate, sdch\r\n";
	//request_stream << "Accept-Language: zh-TW,zh;q=0.8,en-US;q=0.6,en;q=0.4\r\n";

    switch (get_header_method())
	{
	default:
	case HTTP_HEADER_METHOD_GET:
		break;
	case HTTP_HEADER_METHOD_POST:
        if (data_len > 0) request_stream << "Content-Length: " << data_len << "\r\n";
		break;
	case HTTP_HEADER_METHOD_PUT:
        if (data_len > 0) request_stream << "Content-Length: " << data_len << "\r\n";
		break;
	}
    
    if (HTTP_HEADER_CONNECTION_KEEPALIVE == get_header_connection())
        request_stream << "Connection: Keep-Alive\r\n";
    else
        request_stream << "Connection: close\r\n";
    
    if (username_.length() > 0) {
        ss.str(""); ss << username_ << ":" << password_;
        char *auth = _b64encode((char *)ss.str().c_str());
        request_stream << "Authorization: Basic " << auth << "\r\n";
        SAFE_FREE(auth);
    }

    switch (get_header_method())
	{
	default:
	case HTTP_HEADER_METHOD_GET:
		request_stream << "\r\n";
		break;
	case HTTP_HEADER_METHOD_POST:
		request_stream << "\r\n";
		if (data_len > 0) request_stream << data;
		break;
	case HTTP_HEADER_METHOD_PUT:
		request_stream << "\r\n";
		if (data_len > 0) request_stream << data;
		break;
	}
    
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(host_), port_);

    socket().async_connect(ep, 
		boost::bind(&HClient::HandleConnectRequest, shared_from_this(),
			request, boost::asio::placeholders::error));

	return 1;
}

int HClient::SendData(const std::string &host, const std::size_t port, const std::string &path, const std::string &query,
	const std::string &username, const std::string &password)
{
	//DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::SendData. \n", __LINE__));
	if (HTTP_HEADER_CONNECTION_KEEPALIVE == get_header_connection()) {
		boost::shared_ptr<boost::asio::streambuf> request(new boost::asio::streambuf);

		std::ostream request_stream(request.get());
		std::stringstream ss;

		request_stream << "GET " << path << "?" << query << " HTTP/1.0\r\n";
		request_stream << "Host: " << host << "\r\n";
		request_stream << "Accept: */*\r\n";

		request_stream << "Connection: Keep-Alive\r\n";

		if (username.length() > 0) {
			ss.str(""); ss << username << ":" << password;
			char *auth = _b64encode((char *)ss.str().c_str());
			request_stream << "Authorization: Basic " << auth << "\r\n";
			SAFE_FREE(auth);
		}
		request_stream << "\r\n" << std::endl;

		boost::asio::async_write(socket(), *request,
			boost::bind(&HClient::HandleWriteRequest, shared_from_this(),
			request, boost::asio::placeholders::error));
	}
	else
		return 0;

	return 1;
}

int HClient::SendData(const unsigned char *data, unsigned int data_len)
{
	if (HTTP_HEADER_CONNECTION_KEEPALIVE == get_header_connection()) {
		boost::shared_ptr<std::vector<unsigned char> > buff(new std::vector<unsigned char>(data, data + data_len));

		boost::asio::async_write(socket(), boost::asio::buffer(*buff)/*, boost::asio::transfer_exactly(data_len)*/,
			boost::bind(&HClient::HandleKeepWrite, shared_from_this(),
				buff, boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
	}

	return 1;
}

void HClient::HandleConnectRequest(boost::shared_ptr<boost::asio::streambuf> &request, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleConnect. \n", __LINE__));
    
    // james: test HandleConnect.
    //boost::this_thread::sleep(boost::posix_time::millisec(5000));

    if (!err) {
		boost::asio::async_write(socket(), *request,
                                 boost::bind(&HClient::HandleWriteRequest, shared_from_this(),
									request, boost::asio::placeholders::error));
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_connect, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

void HClient::HandleConnect(boost::shared_ptr<std::vector<unsigned char> > &data, const boost::system::error_code &err)
{
	//DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleConnect. \n", __LINE__));

	// james: test HandleConnect.
	//boost::this_thread::sleep(boost::posix_time::millisec(5000));

	if (!err) {

		boost::asio::ip::tcp::no_delay option(true);
		socket().set_option(option);

		boost::asio::async_write(socket(), boost::asio::buffer(*data),
			boost::bind(&HClient::HandleKeepWrite, shared_from_this(),
				data, boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CONNECT_OK, NULL, 0, get_user_arg());
		}
	}
	else {

		DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_connect, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
	}
}

void HClient::HandleWriteRequest(boost::shared_ptr<boost::asio::streambuf> &request, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleWriteRequest. \n", __LINE__));
	
    if (!err) {
		boost::shared_ptr<boost::asio::streambuf> response(new boost::asio::streambuf);

		// james: test HandleWriteRequest.
#if 0
		boost::asio::async_read(socket(), *response, boost::asio::transfer_exactly(32),
			boost::bind(&OhHClient::HandleReadStatusLine, shared_from_this(),
				response, boost::asio::placeholders::error));
#else
        boost::asio::async_read_until(socket(), *response, "\r\n",
                                      boost::bind(&HClient::HandleReadStatusLine, shared_from_this(),
										response, boost::asio::placeholders::error));
#endif
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_write_request, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

void HClient::HandleReadStatusLine(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleReadStatusLine. \n", __LINE__));
    
    if (!err) {

        // Check that response is OK.
		std::istream response_stream(response.get());
        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;
        std::string status_message;
        std::getline(response_stream, status_message);
        if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_read_status_line, Invalid response. \n", __LINE__);

			if (get_action_func()) {
				get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
			}

            return;
        }
        
        if (status_code != 200) {
            DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_read_status_line, status code=%u. \n", __LINE__, status_code);

			if (get_action_func()) {
				get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
			}

            return;
        }

		// Read the response headers, which are terminated by a blank line.
		boost::asio::async_read_until(socket(), *response, "\r\n\r\n",
			boost::bind(&HClient::HandleReadHeaders, shared_from_this(),
				response, boost::asio::placeholders::error));
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_read_status_line, Invalid response. \n", __LINE__);

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

void HClient::HandleReadHeaders(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d HClient::HandleReadHeaders. \n", __LINE__));
    
    if (!err) {
        // Process the response headers.
        std::istream response_stream(response.get());
        std::string header;
        char temp[128];
        std::size_t content_len = 0;
        
        while (std::getline(response_stream, header) && header != "\r") {
            //std::cerr << header << "\n";
            
            std::transform(header.begin(), header.end(), header.begin(), ::toupper);
            if (1 == sscanf(header.c_str(), HTTP_CLIENT_HEADER_BO"%*c%*c%[^\r\n]", temp)) {
                http_headers_[HTTP_CLIENT_HEADER_BO] = temp;
            }
            else if (1 == sscanf(header.c_str(), HTTP_CLIENT_HEADER_TE"%*c%*c%[^\r\n]", temp)) {
                http_headers_[HTTP_CLIENT_HEADER_TE] = temp;
            }
            else if (1 == sscanf(header.c_str(), HTTP_CLIENT_HEADER_SC"%*c%*c%[^\r\n]", temp)) {
                http_headers_[HTTP_CLIENT_HEADER_SC] = temp;
            }
            else if (1 == sscanf(header.c_str(), HTTP_CLIENT_HEADER_CT"%*c%*c%[^\r\n]", temp)) {
                http_headers_[HTTP_CLIENT_HEADER_CT] = temp;
            }
            else if (1 == sscanf(header.c_str(), HTTP_CLIENT_HEADER_CL"%*c%*c%[^\r\n]", temp)) {
                http_headers_[HTTP_CLIENT_HEADER_CL] = temp;
                content_len = boost::lexical_cast<std::size_t>(temp);
            }
        }
        
        std::size_t resi_size = 0;
        if (content_len > 0)
            resi_size = content_len - response->size();
        else
            resi_size = 0;
        
        if (resi_size > 0) {
            boost::asio::async_read(socket(), *response,
                                    boost::asio::transfer_exactly(resi_size),
                                    boost::bind(&HClient::HandleReadContent, shared_from_this(),
                                                response, boost::asio::placeholders::error));
        }
        else {
            if (get_action_func()) {
				get_action_func()(get_uid(), HTTP_ERR_OK, boost::asio::buffer_cast<const unsigned char *>(response->data()), boost::asio::buffer_size(response->data()), get_user_arg());
            }
        }
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_read_headers, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

void HClient::HandleReadContent(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleReadContent. \n", __LINE__));
	
    if (!err) {
        
        if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_OK, boost::asio::buffer_cast<const unsigned char *>(response->data()), boost::asio::buffer_size(response->data()), get_user_arg());
        }
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::handle_read_content, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

void HClient::HandleKeepWrite(boost::shared_ptr<std::vector<unsigned char> > &data, std::size_t bytes, const boost::system::error_code &err)
{
    //DEBUG_TRACE(AVC_DBG, ("#%d OhHClient::HandleKeepWrite. \n", __LINE__));

    if (!err) {
		//jam::check_stream_rate(bytes);
    }
    else {
		
        DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HClient::HandleKeepWrite, err=%s. \n", __LINE__, err.message().c_str());

		if (get_action_func()) {
			get_action_func()(get_uid(), HTTP_ERR_CLOSE, NULL, 0, get_user_arg());
		}
    }
}

////
IHttpClients *CreateIHttpClients()
{
    return static_cast<IHttpClients *> (new HttpClients);
}

void DeleteIHttpClients(IHttpClients *in)
{
    delete in;
}

void HttpClients::Release()
{
    delete this;
}

HttpClients::HttpClients()
:thread_pool_size_(1),
signals_(io_service()),
saving_hc_uid_(0)
{
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif
}

HttpClients::~HttpClients()
{
    StopIoserviceThread();
}

void HttpClients::OnHttpClientMsg(const unsigned int http_uid, _HttpErr http_err, const unsigned char *resdata, const unsigned int resdata_len, const void *user_arg)
{
	do
	{
		switch (http_err)
		{
		case HTTP_ERR_OK:
			break;
		case HTTP_ERR_CONNECT_OK:
			break;
		default:
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "? (%u) HttpClients::OnHttpClientMsg: http_uid=%u, is_need_del_uid. \n", __LINE__, http_uid);
			hclient_map_.erase(http_uid);
			break;
		}

	} while (0);
}

void HttpClients::StartIoservice()
{
    std::vector<boost::shared_ptr<boost::thread> > threads;
    
    signals_.async_wait(boost::bind(&HttpClients::StopIoserviceThread, this));
    
    for (std::size_t i = 0; i < thread_pool_size_; i++) {
        boost::shared_ptr<boost::thread> thread(new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service())));
        threads.push_back(thread);
    }
    
    for (std::size_t i = 0; i < threads.size(); i++) {
        threads[i]->join();
    }
}

void HttpClients::StartIoserviceThread()
{
    if (NULL == io_service_thread_) {
        io_service_thread_.reset(new boost::thread(boost::bind(&HttpClients::StartIoservice, this)));
    }
}

void HttpClients::StopIoserviceThread()
{
    io_service().stop();
    if (io_service_thread_) {
        io_service_thread_->join();
        io_service_thread_.reset();
    }
}

int HttpClients::RequestData(const unsigned char *data, unsigned int data_len)
{
    int err = 1;
    
    /*boost::shared_ptr<HClient> hclient = boost::shared_ptr<HClient> (new HClient(io_service()));
    
    hclient->RequestData(data, data_len);
    
    StartIoserviceThread();*/
    
    return err;
}

unsigned int HttpClients::CreateHttpUid(const std::string &url, const std::string &username, const std::string &password,
                                        _HttpHeaderConnection conn, _HttpHeaderMethod method,
                                        boost::function<HCActionFuncT> action_func, const void *user_arg)
{
    OhUrl u(url);
    boost::function<HCActionFuncT> cb = boost::bind(&HttpClients::OnHttpClientMsg, this, _1, _2, _3, _4, _5);

    boost::shared_ptr<HClient> tmp = boost::shared_ptr<HClient> (new HClient(io_service(), u.host(), u.port(), u.path(), u.query(), username, password));
    tmp->set_header_connection(conn);
    tmp->set_header_method(method);
    tmp->set_action_func(cb);
	tmp->set_user_arg(this);

	{
		boost::mutex::scoped_lock lock(hclient_guard_);

		unsigned int tmp_uid = get_saving_hc_uid();
		if (tmp_uid + 1 == 0) tmp_uid = 1;
		else tmp_uid++;
		set_saving_hc_uid(tmp_uid);
		tmp->set_uid(get_saving_hc_uid());

		hclient_map_[get_saving_hc_uid()] = tmp;
	}
    
    return get_saving_hc_uid();
}

int HttpClients::RequestData(const unsigned int uid, const unsigned char *data, unsigned int data_len)
{
    int ret = 1;

    do {
		HClientMap::iterator iter;
		{
			boost::mutex::scoped_lock lock(hclient_guard_);
			iter  = hclient_map_.find(uid);
			if (iter == hclient_map_.end()) {
                ret = 0;
				// DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HttpClients::RequestData, err=%s. \n", __LINE__, err);
				break;
			}

			if (false == iter->second->has_post_header()) {
                if (!iter->second->RequestData(data, data_len)) {
                    ret = 0;
					hclient_map_.erase(uid);
					break;
				}

				StartIoserviceThread();

				iter->second->set_has_post_header(true);
			}
			else {
				iter->second->SendData(data, data_len);
			}
		}
    } while (0);
    
    return ret;
}

int HttpClients::SendData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password)
{
	int err = 1;

/*	do {
		HClientMap::iterator iter;
		{
			boost::mutex::scoped_lock lock(hclient_guard_);
			iter = hclient_map_.find(uid);
		}

		if (iter == hclient_map_.end()) {
			err = 0;
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HttpClients::SendData, err=%s. \n", __LINE__, err);
			break;
		}

		OhUrl u(url);

		iter->second->SendData(u.host(), u.port(), u.path(), u.query(), username, password);
	} while (0);*/

	return err;
}

int HttpClients::SendData(const unsigned int uid, const unsigned char *data, unsigned int data_len)
{
	int err = 1;

/*	do {
		HClientMap::iterator iter;
		{
			boost::mutex::scoped_lock lock(hclient_guard_);
			iter = hclient_map_.find(uid);
		}

		if (iter == hclient_map_.end()) {
			err = 0;
			DEBUG_TRACE(JAM_DEBUG_LEVEL_WARN, "#%d HttpClients::RequestData, err=%s. \n", __LINE__, err);
			break;
		}

		iter->second->SendData(data, data_len);

	} while (0);*/

	return err;
}

void HttpClients::DeleteHttpUid(const unsigned int uid)
{
	boost::mutex::scoped_lock lock(hclient_guard_);
    hclient_map_.erase(uid);
}

} // namespace jam
