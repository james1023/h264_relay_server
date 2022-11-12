#if !defined __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <string>
#include <map>
#include <boost/lexical_cast.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/shared_array.hpp>
#include <boost/serialization/singleton.hpp>

#include "http-client-i.h"

namespace jam {

#define HTTP_CLIENT_HEADER_BO                                               "BOUNDARY"
#define HTTP_CLIENT_HEADER_TE												"TRANSFER-ENCODING"
#define HTTP_CLIENT_HEADER_SC												"SET-COOKIE"
#define HTTP_CLIENT_HEADER_CT												"CONTENT-TYPE"
#define HTTP_CLIENT_HEADER_CL												"CONTENT-LENGTH"


class HClient: public boost::enable_shared_from_this<HClient>,
                boost::noncopyable
{
private:
    boost::asio::ip::tcp::socket socket_;
	unsigned int uid_;
	void *user_arg_;

	boost::shared_ptr<std::vector<unsigned char> > data_;

	bool has_post_header_;
	bool connect_ok_;

    std::string host_;
    std::size_t port_;
    std::string path_;
    std::string query_;
    std::string username_;
    std::string password_;

    typedef std::map<std::string, std::string> HttpHeaders;
    HttpHeaders http_headers_;
    _HttpHeaderConnection header_connection_;
    _HttpHeaderMethod header_method_;
    
    boost::function<HCActionFuncT> action_func_;

public:
    explicit HClient(boost::asio::io_service &io_service,
                     const std::string &host, const std::size_t port, const std::string &path, const std::string &query,
                     const std::string &username, const std::string &password);
    virtual ~HClient();
    
    boost::asio::ip::tcp::socket &socket() {
        return socket_;
    }

    void set_header_connection(_HttpHeaderConnection in) {
        header_connection_ = in;
    }
    _HttpHeaderConnection get_header_connection() const {
        return header_connection_;
    }

    void set_header_method(_HttpHeaderMethod in) {
        header_method_ = in;
    }
    _HttpHeaderMethod get_header_method() const {
        return header_method_;
    }
    
    boost::function<HCActionFuncT> &get_action_func() {
        return action_func_;
    }
    void set_action_func(boost::function<HCActionFuncT> &in) {
        action_func_ = in;
    }
    
    int RequestData(const unsigned char *data, unsigned int data_len);

	int SendData(const std::string &host, const std::size_t port, const std::string &path, const std::string &query,
		const std::string &username, const std::string &password);
	int SendData(const unsigned char *data, unsigned int data_len);
    
	void HandleConnectRequest(boost::shared_ptr<boost::asio::streambuf> &request, const boost::system::error_code &err);
	void HandleConnect(boost::shared_ptr<std::vector<unsigned char> > &data, const boost::system::error_code &err);
	void HandleWriteRequest(boost::shared_ptr<boost::asio::streambuf> &request, const boost::system::error_code &err);
	void HandleReadStatusLine(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code& err);
    void HandleReadHeaders(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code &err);
	void HandleReadContent(boost::shared_ptr<boost::asio::streambuf> &response, const boost::system::error_code &err);
	void HandleKeepWrite(boost::shared_ptr<std::vector<unsigned char> > &data, std::size_t bytes, const boost::system::error_code &err);

	unsigned int get_uid() const { return uid_; }
	void set_uid(unsigned int val) { uid_ = val; }

	void *get_user_arg() const { return user_arg_; }
	void set_user_arg(void *val) { user_arg_ = val; }

	bool has_post_header() const { return has_post_header_; }
	void set_has_post_header(bool in) { has_post_header_ = in; }

	bool connect_ok() const { return connect_ok_; }
	void set_connect_ok(bool in) { connect_ok_ = in; }
};

class HttpClients:	public IHttpClients, 
					//public boost::enable_shared_from_this<HttpClients>, 
                    public boost::serialization::singleton<HttpClients>/*,
                    boost::noncopyable*/
{
private:
	boost::mutex interface_guard_;
	boost::mutex hclient_guard_;

    boost::asio::io_service io_service_;
    std::size_t thread_pool_size_;
    boost::shared_ptr<boost::thread> io_service_thread_;
    
    boost::asio::signal_set signals_;
    
    unsigned int saving_hc_uid_;
    typedef std::map<unsigned int, boost::shared_ptr<HClient> > HClientMap;
    HClientMap hclient_map_;
    
public:
	explicit HttpClients();
	virtual ~HttpClients();
    
    virtual void Release();
    
    boost::asio::io_service &io_service() {
        return io_service_;
    }
    
    void StartIoservice();
    void StartIoserviceThread();
    void StopIoserviceThread();
    
    int RequestData(const unsigned char *data = NULL, unsigned int data_len = 0);
    
    unsigned int get_saving_hc_uid() {
        return saving_hc_uid_;
    }
    void set_saving_hc_uid(const unsigned int in) {
        saving_hc_uid_ = in;
    }
    
    unsigned int CreateHttpUid(const std::string &url, const std::string &username, const std::string &password,
                               _HttpHeaderConnection conn = HTTP_HEADER_CONNECTION_CLOSE, _HttpHeaderMethod method = HTTP_HEADER_METHOD_GET,
                               boost::function<HCActionFuncT> action_func = 0, const void *user_arg = 0);
    int RequestData(const unsigned int uid, const unsigned char *post_data = NULL, unsigned int post_len = 0);
	int SendData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password);
	int SendData(const unsigned int uid, const unsigned char *data, unsigned int data_len);
    void DeleteHttpUid(const unsigned int uid);

	void OnHttpClientMsg(const unsigned int http_uid, _HttpErr http_err, const unsigned char *resdata, const unsigned int resdata_len, const void *user_arg);
};

} // namespace jam

#endif // __HTTP_CLIENT_H__
