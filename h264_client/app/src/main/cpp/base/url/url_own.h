#pragma once

#include <string>

namespace base {

extern int parseUrl(const char *url, char *sche, char *ip, unsigned int *portNum, const char **urlSuffix);
extern int ParserUrlHandler(const char *pchUrlText, char *pchIPAddress, unsigned long *pdwPort, char **ppchUri, char **ppchUriCmd);
extern int parseHTTPUrl(const char *url, char *ip, unsigned int *portNum, const char **urlSuffix);
extern int ParserHTTPUrlHandler(const char *pchUrlText, char *pchIPAddress, unsigned int *pdwPort, char **ppchUri, char **ppchUriCmd);
extern int replaceUrlUri(char *pchUrl, const char *pchNewUri);

extern int replaceUrlScheme(char *pchUrl, const char *pchUrlScheme);
extern char *getAvtechCgiUrl(const char *pchServerName, const unsigned int dwPort, char *pchUri);
extern int IsDNSName(char *pchIP);
extern char *url_decode(const char *str);
extern std::string url_encode(const std::string &str);

#define b64is7bit(c)  ((c) > 0x7f ? 0 : 1)  /* Valid 7-Bit ASCII character?*/
#define b64blocks(l) (((l) + 2) / 3 * 4 + 1)/* Length rounded to 4 byte block.*/
#define b64octets(l)  ((l) / 4  * 3 + 1)    /* Length rounded to 3 byte octet.*/ 
extern char* _b64encode(char *s);
extern char* _b64decode(char *s);
extern unsigned char _b64valid(unsigned char *c);
extern char* _b64isnot(char *p, char *b);
extern char *_b64buffer(char *s, unsigned char f);

// *ppchRetData cannot be free by user.
extern int TestONVIFRequest(char *pchInData, char *pchIP, unsigned int dwPort, char **ppchRetData);

class OhUrl
{
private:
    std::string protocol_;
    std::string host_;
    std::size_t port_;
    std::string path_;
    std::string query_;
    
public:
    OhUrl(const std::string& url);
    virtual ~OhUrl();
    
    std::string &protocol() { return protocol_; }
    std::string &host() { return host_; }
    std::size_t &port() { return port_; }
    std::string &path() { return path_; }
    std::string &query() { return query_; }
};

} // namespace base
