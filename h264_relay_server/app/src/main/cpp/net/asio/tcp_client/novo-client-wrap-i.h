#ifndef HTTP_CLIENT_WRAP_I_H
#define HTTP_CLIENT_WRAP_I_H

#include <string>
#include <boost/function.hpp>

#include "ibase.h"
#include "interface.h"
#include "http-client.h"

namespace jam {
    
//
// Interface.
//
static BOMID BOID_IWRAP_HTTP_CLIENT = {0x99981234, 0xcccc, 0xbbbb, 0x23, 0xab, 0xff, 0xef, 0x99, 0x88, 0xbb, 0xaa};
    
class JAM_INTERFACE_LIB_API IWrapHttpClients: public IBase
{
public:
	//virtual void SetHeaderConnection(const unsigned int uid, _OhHttpHeaderConnection in) = 0;

	virtual int RequestData(const std::string &url, const std::string &username, const std::string &password, boost::function<OhHCActionFuncT> action_func,
		_OhHttpHeaderMethod method = OH_HTTP_HEADER_METHOD_GET, const unsigned char *data = NULL, unsigned int data_len = 0) = 0;

	virtual unsigned int CreateHttpUid(_OhHttpHeaderConnection conn, boost::function<OhHCActionFuncT> action_func, const void *user_arg) = 0;
	virtual int RequestData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password,
		_OhHttpHeaderMethod method = OH_HTTP_HEADER_METHOD_GET, const unsigned char *data = NULL, unsigned int data_len = 0) = 0;
	virtual int SendData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password) = 0;
	virtual int SendData(const unsigned int uid, const unsigned char *data, unsigned int data_len) = 0;
	virtual void DeleteHttpUid(const unsigned int uid) = 0;
};
    
class WrapHttpClients : public IWrapHttpClients
{
private:
    boost::shared_ptr<HttpClients> http_clients_;
    
public:
    explicit WrapHttpClients()
    {
        if (NULL == http_clients_) {
            http_clients_.reset(new HttpClients());
        }
    }
    
    virtual ~WrapHttpClients()
    {
        
    }
    
    void Release()
    {
        delete this;
    }
    
    BASE_ERR QueryInterface(REFBOMID riid, void **ppv)
    {
        if (IsEqualBOMID(riid, BOID_IWRAP_HTTP_CLIENT)) {
            *ppv = (IBase *)this;
            return BASE_OK;
        }
        else  {
            return BASE_PARAM_ERROR;
        }
        
        return BASE_PARAM_ERROR;
    }

	/*virtual void SetHeaderConnection(const unsigned int uid, _OhHttpHeaderConnection in) {
		return oh_http_clients_->SetHeaderConnection(uid, in);
	}*/
    
	virtual int RequestData(const std::string &url, const std::string &username, const std::string &password, boost::function<HCActionFuncT> action_func,
		_HttpHeaderMethod method = HTTP_HEADER_METHOD_GET, const unsigned char *data = NULL, unsigned int data_len = 0) {
		return http_clients_->RequestData(url, username, password, action_func,
			method, data, data_len);
    }

	virtual unsigned int CreateHttpUid(_OhHttpHeaderConnection conn, boost::function<OhHCActionFuncT> action_func, const void *user_arg) {
		return oh_http_clients_->CreateHttpUid(conn, action_func, user_arg);
	}

	virtual int RequestData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password,
		_OhHttpHeaderMethod method = OH_HTTP_HEADER_METHOD_GET, const unsigned char *data = NULL, unsigned int data_len = 0) {
		return oh_http_clients_->RequestData(uid, url, username, password,
			method, data, data_len);
	}

	virtual int SendData(const unsigned int uid, const std::string &url, const std::string &username, const std::string &password) {
		return oh_http_clients_->SendData(uid, url, username, password);
	}

	virtual int SendData(const unsigned int uid, const unsigned char *data, unsigned int data_len) {
		return oh_http_clients_->SendData(uid, data, data_len);
	}

	virtual void DeleteHttpUid(const unsigned int uid) {
		return oh_http_clients_->DeleteHttpUid(uid);
	}
};
    
} // namespace jam 

#endif	// HTTP_CLIENT_WRAP_I_H