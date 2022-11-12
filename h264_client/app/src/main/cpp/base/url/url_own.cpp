
#include <cassert>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <functional>
#include <algorithm>

#include "base/url/url_own.h"
#include "base/url/url.hpp"
#include "base/debug-msg-c.h"

namespace base {

#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#define _strncasecmp _strnicmp
#else
#define _strncasecmp strncasecmp
#endif

int parseUrl(const char *url, char *sche, char *ip, unsigned int *portNum, const char **urlSuffix)
{
	do 
	{
		// Parse the URL as "http://<address>:<port>/<etc>"
		// (with ":<port>" and "/<etc>" optional)
		// Also, skip over any "<username>[:<password>]@" preceding <address>
		unsigned const prefixLength = 0;
/*
		char const* prefix = "http://";
		if (0 != vm_string_strnicmp(url, prefix, prefixLength)) 
		{
			// URL is not of the form "http://"
			break;
		}
*/
		if (sche) {
			sscanf(url, "%[^:]", sche);
		}

		unsigned const parseBufferSize = 100;
		char parseBuffer[parseBufferSize]; parseBuffer[0] = 0x00;
		const char *from = &url[prefixLength];

		if (NULL != (from=strstr(from, "//"))) {
			from += 2;
		}
		else {
			from = &url[prefixLength];
		}

		// Skip over any "<username>[:<password>]@"
		// (Note that this code fails if <password> contains '@' or '/', but
		// given that these characters can also appear in <etc>, there seems to
		// be no way of unambiguously parsing that situation.)
		const char *from1 = from;
		while ('\0' != *from1 && '/' != *from1) 
		{
			if ('@' == *from1) 
			{
				++from1;
				from = from1;
				break;
			}
			++from1;
		}

		char *to = &parseBuffer[0];
		unsigned i;
		for (i = 0; i < parseBufferSize; ++i) 
		{
			if ('\0' == *from || ':' == *from || '/' == *from) 
			{
				// We've completed parsing the address
				*to = '\0';
				break;
			}
			*to++ = *from++;
		}
		if (i == parseBufferSize) 
		{
			// URL is too long
			break;
		}

		if (ip)
			strcpy(ip, parseBuffer);

		// james: 121202 it don't modify port while parsing no port.
		//if (portNum && 0 == *portNum) *portNum = 80; // default value

		char nextChar = *from;
		if (':' == nextChar) 
		{
			int portNumInt = 80;
			if (1 != sscanf(++from, "%d", &portNumInt))
			{
				// No port number follows ':'
				break;
			}
			if (portNumInt < 1 || portNumInt > 65535) 
			{
				// Bad port number
				break;
			}
			if (portNum) *portNum = portNumInt;
			while (*from >= '0' && *from <= '9') ++from; // skip over port number
		}

		// The remainder of the URL is the suffix:
		if (urlSuffix) *urlSuffix = from;

		return true;
	} while (0);

	return false;
}

int ParserUrlHandler(const char *pchUrlText, char *pchIPAddress, unsigned long *pdwPort, char **ppchUri, char **ppchUriCmd)
{
	int ret = true;

	do 
	{
		unsigned int Port = 80;
		if (true != parseUrl(pchUrlText, NULL, pchIPAddress, &Port, (const char **)ppchUri))
		{
			DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "??? #%d ParserUrlHandler: error. \n", __LINE__);
			ret = false;
			break;
		}
		*pdwPort = Port;

		char *pchPtr = NULL;
		if (NULL != ppchUriCmd &&
			NULL != ppchUri &&
			NULL != (pchPtr=strstr(*ppchUri, "?")))
		{
			*ppchUriCmd = pchPtr+1;
		}

	} while (0);

	return ret;
}

int parseHTTPUrl(const char *url, char *ip, unsigned int *portNum, const char **urlSuffix)
{
	do 
	{
		// Parse the URL as "http://<address>:<port>/<etc>"
		// (with ":<port>" and "/<etc>" optional)
		// Also, skip over any "<username>[:<password>]@" preceding <address>
		char const* prefix = "http://";
		unsigned const prefixLength = 7;
		if (0 != _strncasecmp((char *)url, (char *)prefix, strlen(prefix)))
		{
			// URL is not of the form "http://"
			break;
		}

		unsigned const parseBufferSize = 100;
		char parseBuffer[parseBufferSize]; parseBuffer[0] = 0x00;
		const char *from = &url[prefixLength];

		// Skip over any "<username>[:<password>]@"
		// (Note that this code fails if <password> contains '@' or '/', but
		// given that these characters can also appear in <etc>, there seems to
		// be no way of unambiguously parsing that situation.)
		const char *from1 = from;
		while ('\0' != *from1 && '/' != *from1) 
		{
			if ('@' == *from1) 
			{
				from = ++from1;
				break;
			}
			++from1;
		}

		char* to = &parseBuffer[0];
		unsigned i;
		for (i = 0; i < parseBufferSize; ++i) 
		{
			if ('\0' == *from || ':' == *from || '/' == *from) 
			{
				// We've completed parsing the address
				*to = '\0';
				break;
			}
			*to++ = *from++;
		}
		if (i == parseBufferSize) 
		{
			// URL is too long
			break;
		}

		strcpy(ip, parseBuffer);

		*portNum = 80; // default value
		char nextChar = *from;
		if (':' == nextChar) 
		{
			int portNumInt;
			if (1 != sscanf(++from, "%d", &portNumInt))
			{
				// No port number follows ':'
				break;
			}
			if (portNumInt < 1 || portNumInt > 65535) 
			{
				// Bad port number
				break;
			}
			*portNum = portNumInt;
			while (*from >= '0' && *from <= '9') ++from; // skip over port number
		}

		// The remainder of the URL is the suffix:
		if (NULL != urlSuffix) *urlSuffix = from;

		return true;
	} while (0);

	return false;
}

int ParserHTTPUrlHandler(const char *pchUrlText, char *pchIPAddress, unsigned int *pdwPort, char **ppchUri, char **ppchUriCmd)
{
	int ret = true;

	do 
	{
		unsigned int Port					= 0;
		if (true != parseHTTPUrl(pchUrlText, pchIPAddress, &Port, (const char **)ppchUri))
		{
			ret = false;
			break;
		}
		*pdwPort = Port;

		char *pchPtr = NULL;
		if (NULL != ppchUri &&
			NULL != ppchUriCmd &&
			NULL != (pchPtr=strstr(*ppchUri, "?")))
		{
			*ppchUriCmd = pchPtr+1;
		}

	} while (0);

	return ret;
}

/*int replaceUrlScheme(char *pchUrl, const char *pchUrlScheme)
{
	int ret = TRUE;
	char *pchDomain = NULL;
	DWORD dwPort = 0;
	char *pchUri = NULL;

	do 
	{
		pchDomain = (char *)malloc(128);
		if (TRUE != ParserUrlHandler(pchUrl, pchDomain, &dwPort, &pchUri, NULL)) {
			DEBUG_TRACE(GETBUG_DBG, ("??? #%d ParserUrlHandler: error. \n", __LINE__));
			ret = FALSE;
			break;
		}

		sprintf(pchDomain, "%s:%lu%s", pchDomain, dwPort, pchUri);
		sprintf(pchUrl, "%s://%s", pchUrlScheme, pchDomain);
	} while (0);

	SAFE_FREE(pchDomain);

	return ret;
}

int replaceUrlUri(char *pchUrl, const char *pchNewUri)
{
	int ret = TRUE;
	char *pchDomain = NULL;
	DWORD dwPort = 0;
	char *pchUri = NULL;

	do 
	{
		pchDomain = (char *)malloc(128);

		//if (TRUE != ParserUrlHandler(pchUrl, pchDomain, &dwPort, &pchUri, NULL)) 
		{
			DEBUG_TRACE(GETBUG_DBG, ("??? #%d ParserUrlHandler: error. \n", __LINE__));
			ret = FALSE;
			break;
		}

		*pchUri = 0;
		strcat(pchUrl, pchNewUri);
	} while (0);

	SAFE_FREE(pchDomain);

	return ret;
}
*/

char *getAvtechCgiUrl(const char *pchServerName, const unsigned int dwPort, char *pchUri)
{
	if (NULL == pchServerName || NULL == pchUri)
		return NULL;

	typedef std::map<std::string, std::string> _mapConfigNams;
	_mapConfigNams mapConfigNams;

	mapConfigNams["Login.cgi"]							= "cgi-bin/guest/Login.cgi";
	mapConfigNams["Capability.cgi"]						= "cgi-bin/nobody/Capability.cgi";
	mapConfigNams["Machine.cgi"]						= "cgi-bin/nobody/Machine.cgi";
	mapConfigNams["Config.cgi"]							= "cgi-bin/user/Config.cgi";
	mapConfigNams["Serial.cgi"]							= "cgi-bin/user/Serial.cgi";
	mapConfigNams["PTZ.cgi"]							= "cgi-bin/user/PTZ.cgi";

	// 20160831 james: add ipv6 url.
#if 1
	std::stringstream ss;

	std::string base_uri = pchUri;
	int nPos = 0;
	if (std::string::npos == base_uri.find("cgi-bin"))
		for (_mapConfigNams::iterator iter = mapConfigNams.begin(); iter != mapConfigNams.end(); iter++) {
			if ((nPos = base_uri.find(iter->first)) >= 0) {
				base_uri.replace(nPos, strlen(iter->first.c_str()), iter->second);
				break;
			}
		}

	Url u;
	try {
		u.clear().scheme("http").host(pchServerName).port(dwPort);
		ss.str(""); ss << u.str() << "/" << base_uri;
	}
	catch (...) {
		DEBUG_TRACE(JAM_DEBUG_LEVEL_ERROR, "#%d getAvtechCgiUrl, url error. \n", __LINE__);
		return NULL;
	}

    return strdup(ss.str().c_str());
#else
	std::string strBaseUri;
	std::stringstream sstrTemp;
	sstrTemp << "http://" << pchServerName << ":" << dwPort << "/" << pchUri;
	strBaseUri = sstrTemp.str();

	int nPos = 0;
	if (NULL == strstr(pchUri, "cgi-bin"))
		for (_mapConfigNams::iterator iter=mapConfigNams.begin(); iter != mapConfigNams.end(); iter++) {
			if ((nPos=strBaseUri.find(iter->first)) >= 0) {
				strBaseUri.replace(nPos, strlen(iter->first.c_str()), iter->second);
				break;
			}
		}

	mapConfigNams.clear();

	return _strdup(strBaseUri.c_str());
#endif
}

int IsDNSName(char *pchIP)
{
	int length			= (int)strlen(pchIP);

	for(int i=0 ; i < length ; i++)
	{
		if ( (pchIP[i] == '.') || ( (pchIP[i] <= 0x39) && (pchIP[i] >= 0x30 ) ) )
		{
		}
		else
		{
			return true;
		}
	}

	return false;
}

/* Converts a hex character to its integer value */
char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_decode(const char *str) {
	char *pstr = (char *)str, *buf = (char *)malloc(strlen(str) + 1), *pbuf = buf;
	while (*pstr) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		} else if (*pstr == '+') { 
			*pbuf++ = ' ';
		} else {
			*pbuf++ = *pstr;
		}
		pstr++;
	}
	*pbuf = '\0';
	return buf;
}

unsigned char ToHex(unsigned char x)
{
    return  x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}

std::string url_encode(const std::string &value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		std::string::value_type c = (*i);
        
        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        
        // Any other characters are percent-encoded
        escaped << '%' << std::setw(2) << int((unsigned char) c);
    }
    
    return escaped.str();
}

/*
**************************************************************************
* Copyright 2004 AV TECH CO., LTD. ALL RIGHTS RESERVED.
* This software is provided under license and contains proprietary and
* confidential material which is the property of AV TECH.
*
* FileName     	: DynBase64.c
* Description  	: base64 encoding and decodong
* Reference		: Source code by Marc Niegowski  2001.11.14
* author		: Marc Niegowski
* rewrite		: Bryan Chen
*
* Version control:
*  $Revision: 1.1.1.1 $    Date: 2005/01/06      Bryan
*
**************************************************************************
*/


/*---------------------------------------------------------------------------*/
/* b64encode - Encode a 7-Bit ASCII string to a Base64 string.               */
/* ===========================================================               */
/*                                                                           */
/* Call with:   char *  - The 7-Bit ASCII string to encode.                  */
/*                                                                           */
/* Returns:     char* - new character array that contains the new base64     */
/*              encoded string -- call free() on the resultant pointer!      */
/*              modified for formatting from original 10/24/2001 by ces      */
/*---------------------------------------------------------------------------*/
char* _b64encode(char *s)
{
	static  
		const										/* Base64 Index into encoding*/
		unsigned char  pIndex[] = {					/* and decoding table.*/
			0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
			0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
			0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
			0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
			0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
			0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
			0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33,
			0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f
	};


	int	l = (int)strlen(s);
	int	x = 0;
	char   *b, *p;

	while (x < l) {
		if (!b64is7bit((unsigned char) *(s + x))) {
            return 0;
		}
		x++;
	}

	if (!(b = _b64buffer(s, true)))
        return 0;

	memset(b, 0x3d, b64blocks(l) - 1);

	p = b;
	x = 0;

	while (x < (l - (l % 3))) {
		*b++ = pIndex[  s[x]             >> 2];
		*b++ = pIndex[((s[x]     & 0x03) << 4) + (s[x + 1] >> 4)];
		*b++ = pIndex[((s[x + 1] & 0x0f) << 2) + (s[x + 2] >> 6)];
		*b++ = pIndex[  s[x + 2] & 0x3f];
		x += 3;
	}

	if (l - x) {
		*b++ = pIndex[s[x] >> 2];

		if (l - x == 1)
			*b = pIndex[ (s[x] & 0x03) << 4];
		else {
			*b++ = pIndex[((s[x]     & 0x03) << 4) + (s[x + 1] >> 4)];
			*b   = pIndex[ (s[x + 1] & 0x0f) << 2];
		}
	}

	return p;
}

/*---------------------------------------------------------------------------*/
/* b64decode - Decode a Base64 string to a 7-Bit ASCII string.               */
/* ===========================================================               */
/*                                                                           */
/* Call with:   char *  - The Base64 string to decode.                       */
/*                                                                           */
/* Returns:     char* - new character array that contains the new base64     */
/*              decoded string -- call free() on the resultant pointer!      */
/*              modified from original 10/24/2001 by ces                     */
/*---------------------------------------------------------------------------*/
char* _b64decode(char *s)
{
	int				l = (int)strlen(s);                  
	char			*b, *p;                          
	unsigned char	c = 0;                          
	int				x = 0;                          
	int				y = 0;

	static
		const
		char	pPad[] = {0x3d, 0x3d, 0x3d, 0x00};

	if (l % 4)
		return _b64isnot(s, NULL);           

	if ((b = strchr(s, pPad[0])) != 0) {
		if  ((b - s) < (l - 3))
			return _b64isnot(s, NULL);
		else
			if (strncmp(b, (char *) pPad + 3 - (s + l - b), s + l - b))
				return _b64isnot(s, NULL);
	}

	if (!(b = _b64buffer(s, false)))
        return 0;

	p = s;
	x = 0;

	while ((c = *s++)) {
		if (c == pPad[0])
			break;

		if (!_b64valid(&c))
			return _b64isnot(s, b);

		switch(x % 4) {
		case 0:
			b[y] = c << 2;
			break;
		case 1:
			b[y] |=  c >> 4;

			if (!b64is7bit((unsigned char) b[y++])) 
				return _b64isnot(s, b);      

			b[y] = (c & 0x0f) << 4;
			break;
		case 2:
			b[y] |=  c >> 2;

			if (!b64is7bit((unsigned char) b[y++]))
				return _b64isnot(s, b);

			b[y] = (c & 0x03) << 6;
			break;
		case 3:
			b[y] |=  c;

			if (!b64is7bit((unsigned char) b[y++]))
				return _b64isnot(s, b);
		}
		x++;
	}

	return b;
}

/*---------------------------------------------------------------------------*/
/* b64valid - validate the character to decode.                              */
/* ============================================                              */
/*                                                                           */
/* Checks whether the character to decode falls within the boundaries of the */
/* Base64 decoding table.                                                    */
/*                                                                           */
/* Call with:   char    - The Base64 character to decode.                    */
/*                                                                           */
/* Returns:     bool    - True (!0) if the character is valid.               */
/*                        False (0) if the character is not valid.           */
/*---------------------------------------------------------------------------*/
unsigned char _b64valid(unsigned char *c)
{
	static
		const
		unsigned char   pBase64[] = {                  
			0x3e, 0x7f, 0x7f, 0x7f, 0x3f, 0x34, 0x35, 0x36,
			0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x7f,
			0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x01,
			0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
			0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
			0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
			0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x1a, 0x1b,
			0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
			0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
			0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
	};

	if ((*c < 0x2b) || (*c > 0x7a))
		return false;

	if ((*c = pBase64[*c - 0x2b]) == 0x7f)
		return false;

	return true;
}

/*---------------------------------------------------------------------------*/
/* b64isnot - Display an error message and clean up.                         */
/* =================================================                         */
/*                                                                           */
/* Call this routine to display a message indicating that the string being   */
/* decoded is an invalid Base64 string and de-allocate the decoding buffer.  */
/*                                                                           */
/* Call with:   char *  - Pointer to the Base64 string being decoded.        */
/*              char *  - Pointer to the decoding buffer or NULL if it isn't */
/*                        allocated and doesn't need to be de-allocated.     */
/*                                                                           */
/* Returns:     bool    - True (!0) if the character is valid.               */
/*                        False (0) if the character is not valid.           */
/*---------------------------------------------------------------------------*/
char* _b64isnot(char *p, char *b)
{
	if (b)
		free(b);

	return  NULL;
}

/*---------------------------------------------------------------------------*/
/* b64buffer - Allocate the decoding or encoding buffer.                     */
/* =====================================================                     */
/*                                                                           */
/* Call this routine to allocate an encoding buffer in 4 byte blocks or a    */
/* decoding buffer in 3 byte octets.  We use "calloc" to initialize the      */
/* buffer to 0x00's for strings.                                             */
/*                                                                           */
/* Call with:   char *  - Pointer to the string to be encoded or decoded.    */
/*              bool    - True (!0) to allocate an encoding buffer.          */
/*                        False (0) to allocate a decoding buffer.           */
/*                                                                           */
/* Returns:     char *  - Pointer to the buffer or NULL if the buffer        */
/*                        could not be allocated.                            */
/*---------------------------------------------------------------------------*/
char *_b64buffer(char *s, unsigned char f)
{
	int	l = (int)strlen(s);
	int	size;
	//INT8U	err;
	char	*b;

	if (!l)
		return  NULL;

	size = f ? b64blocks(l) : b64octets(l);
	size *= sizeof(char);
	b = (char *)malloc(size);
	memset(b, 0, size);

	return b;
}

OhUrl::OhUrl(const std::string &url)
{
    const std::string prot_end("://");
	std::string::const_iterator prot_i = search(url.begin(), url.end(),
                                           prot_end.begin(), prot_end.end());
    protocol_.reserve(distance(url.begin(), prot_i));
    transform(url.begin(), prot_i,
              back_inserter(protocol_),
			  // james: removed in c++17.
#if 0
			  std::ptr_fun<int,int>(tolower));
#else
			  [](int c) {return std::tolower(c);});
#endif
    if (prot_i == url.end())
        return;
    advance(prot_i, prot_end.length());
    
	std::string::const_iterator path_i = find(prot_i, url.end(), ':');
    if (path_i != url.end()) {
        host_.reserve(distance(prot_i, path_i));
        transform(prot_i, path_i,
                  back_inserter(host_),
				  // james: removed in c++17.
#if 0
				  std::ptr_fun<int,int>(tolower));
#else
				  [](int c) {return std::tolower(c);});
#endif
        
		std::string::const_iterator port_i = find(path_i, url.end(), '/');
		std::string port_s;
        port_s.assign(path_i+1, port_i);
        port_ = (std::size_t)std::stoi(port_s);
        path_i = port_i;
    }
    else {
        path_i = find(prot_i, url.end(), '/');
        host_.reserve(distance(prot_i, path_i));
        transform(prot_i, path_i,
                  back_inserter(host_),
				  // james: removed in c++17.
#if 0
				  std::ptr_fun<int,int>(tolower));
#else
				  [](int c) {return std::tolower(c);});
#endif
        
        port_ = 80;
    }
    
	std::string::const_iterator query_i = find(path_i, url.end(), '?');
    path_.assign(path_i, query_i);
    if (query_i != url.end())
        ++query_i;
    query_.assign(query_i, url.end());
}

OhUrl::~OhUrl()
{

}

} // namespace base
