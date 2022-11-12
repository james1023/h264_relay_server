#ifndef IHTTP_CLIENT_H
#define IHTTP_CLIENT_H

#include <string>
#include <boost/function.hpp>

#include "base/interface-c.h"

namespace jam {

typedef enum __HttpHeaderMethod
{
	HTTP_HEADER_METHOD_GET = 0,
	HTTP_HEADER_METHOD_POST,
	HTTP_HEADER_METHOD_PUT,
	HTTP_HEADER_METHOD_HAS_HEADER,
	HTTP_HEADER_METHOD_NONEED
} _HttpHeaderMethod;

typedef enum __HttpHeaderConnection
{
	HTTP_HEADER_CONNECTION_CLOSE = 0,
	HTTP_HEADER_CONNECTION_KEEPALIVE = 1
} _HttpHeaderConnection;

typedef enum __HttpErr
{
	HTTP_ERR_CLOSE = 0,
	HTTP_ERR_OK,
	HTTP_ERR_CONNECT_OK,
	
} _HttpErr;

typedef void (HCActionFuncT)(const unsigned int http_uid, _HttpErr http_err, const unsigned char *resdata, const unsigned int resdata_len, const void *user_arg);
    
class JAM_INTERFACE_LIB_API IHttpClients
{
public:
    virtual int RequestData(const unsigned char *data = NULL, unsigned int data_len = 0) = 0;
   
    virtual unsigned int CreateHttpUid(const std::string &url, const std::string &username, const std::string &password,
                                       _HttpHeaderConnection conn = HTTP_HEADER_CONNECTION_CLOSE, _HttpHeaderMethod method = HTTP_HEADER_METHOD_GET,
                                       boost::function<HCActionFuncT> action_func = 0, const void *user_arg = 0) = 0;
    virtual int RequestData(const unsigned int uid, const unsigned char *data = NULL, unsigned int data_len = 0) = 0;
	virtual int SendData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password) = 0;
	virtual int SendData(const unsigned int uid, const unsigned char *data, unsigned int data_len) = 0;
	virtual void DeleteHttpUid(const unsigned int uid) = 0;
};
    
JAM_INTERFACE_LIB_API IHttpClients *CreateIHttpClients();
JAM_INTERFACE_LIB_API void DeleteIHttpClients(IHttpClients *in);
    
} // namespace jam

#endif	// IHTTP_CLIENT_H
