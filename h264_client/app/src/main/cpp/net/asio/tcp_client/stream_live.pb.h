#pragma once

#include <string>

namespace net {
class StreamMessage;
}  // namespace net

namespace net {

enum StreamMessage_TRANSPORT_PROTOCOL {
  StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_RTSP_MULTICAST = 0,
  StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_NOVO_TCP = 1,
  StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_RTSP_TCP_PATH = 2
};

class StreamMessage {

private:
	std::string url_;
	std::string username_;
	std::string password_;

	//StreamMessage_TRANSPORT_PROTOCOL transport_protocol_;
	int transport_protocol_;

 public:
	 StreamMessage() {};
	 virtual ~StreamMessage() {};

	typedef StreamMessage_TRANSPORT_PROTOCOL TRANSPORT_PROTOCOL;
	static const TRANSPORT_PROTOCOL TRANSPORT_PROTOCOL_RTSP_MULTICAST =
	StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_RTSP_MULTICAST;
	static const TRANSPORT_PROTOCOL TRANSPORT_PROTOCOL_NOVO_TCP =
	StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_NOVO_TCP;
	static const TRANSPORT_PROTOCOL TRANSPORT_PROTOCOL_RTSP_TCP_PATH =
	StreamMessage_TRANSPORT_PROTOCOL_TRANSPORT_PROTOCOL_RTSP_TCP_PATH;
  
	const ::std::string& url() const;
	void set_url(const std::string &value);

	const ::std::string& username() const;
	void set_username(const ::std::string& value);

	const ::std::string& password() const;
	void set_password(const ::std::string& value);
 
	::net::StreamMessage_TRANSPORT_PROTOCOL transport_protocol() const;
	void set_transport_protocol(::net::StreamMessage_TRANSPORT_PROTOCOL value);
};
// ===================================================================

inline const ::std::string& StreamMessage::url() const {
	return url_;
}
inline void StreamMessage::set_url(const ::std::string& value) {
	url_ = value;
}

inline const ::std::string& StreamMessage::username() const {
	return username_;
}
inline void StreamMessage::set_username(const ::std::string& value) {
	username_ = value;
}

inline const ::std::string& StreamMessage::password() const {
	return password_;
}
inline void StreamMessage::set_password(const ::std::string& value) {
	password_ = value;
}

inline ::net::StreamMessage_TRANSPORT_PROTOCOL StreamMessage::transport_protocol() const {
	return static_cast< ::net::StreamMessage_TRANSPORT_PROTOCOL >(transport_protocol_);
}
inline void StreamMessage::set_transport_protocol(::net::StreamMessage_TRANSPORT_PROTOCOL value) {
	transport_protocol_ = value;
}

}  // namespace net