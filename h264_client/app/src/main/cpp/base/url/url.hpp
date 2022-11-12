#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <ostream>
#include <utility>

class Url {
public:
    // Exception thut may be thrown when decoding an URL or an assigning value
    class parse_error: public std::invalid_argument {
    public:
        parse_error(const std::string &reason) : std::invalid_argument(reason) {}
    };

    // Exception that may be thrown when building an URL
    class build_error: public std::runtime_error {
    public:
        build_error(const std::string &reason) : std::runtime_error(reason) {}
    };

    // Default constructor
    Url() : m_parse(true),m_built(true),m_ip_v(-1), port_(0) {}

    // Copy initializer constructor
    Url(const Url &url) : m_ip_v(-1), port_(0) {assign(url);}

    // Move constructor, C++11 support.
    //Url(Url&& url) : m_ip_v(-1) {assign(std::move(url));}

    // Construct Url with the given string
    Url(const std::string &url_str) : m_url(url_str),m_parse(false),m_built(false),m_ip_v(-1), port_(0) {}

    // Assign the given URL string
    Url &operator=(const std::string &url_str) {return str(url_str);}

    // Assign the given Url object
    Url &operator=(const Url &url) {assign(url); return *this;}

    // Move the given Url object, C++11 support.
    //Url &operator=(Url&& url) {assign(std::move(url)); return *this;}

    // Clear the Url object
    Url &clear();

    // Build Url if needed and return it as string
    std::string str() {if(!m_built) build_url(); return m_url;}

    // Set the Url to the given string. All fields are overwritten
    Url& str(const std::string &url_str) {m_url=url_str; m_built=m_parse=false; return *this;}

    // Get scheme
    const std::string& scheme() {lazy_parse(); return m_scheme;}

    // Set scheme
    Url &scheme(const std::string& s);

    // Get user info
    const std::string& user_info() {lazy_parse(); return m_user;}

    // Set user info
    Url &user_info(const std::string& s);

    // Get host
    const std::string& host() {lazy_parse(); return m_host;}

    // Set host
    Url &host(const std::string& h, uint8_t ip_v=0);

    // Get host IP version: 0=name, 4=IPv4, 6=IPv6, -1=undefined
    std::int8_t ip_version() {lazy_parse(); return m_ip_v;}

    // Get port
    const std::string& port() { lazy_parse(); return m_port; }
	const int port_value() { lazy_parse(); return port_; }

    // Set Port given as string
    Url &port(const std::string& str);

    // Set port given as a 16bit unsigned integer
    Url &port(std::uint16_t num) {return port(std::to_string(num));}

    // Get path
    const std::string& path() {lazy_parse(); return m_path;}

    // Set path
    Url &path(const std::string& str);

	// Get uri
	const std::string& uri() { lazy_parse(); return uri_; }

    class KeyVal {
    public:
        // Default constructor
        KeyVal() {}

        // Construct with provided Key and Value strings
        KeyVal(const std::string &key, const std::string &val) : m_key(key),m_val(val) {}

        // Construct with provided Key string, val will be empty
        KeyVal(const std::string &key) : m_key(key) {}

        // Equality test operator
        bool operator==(const KeyVal &q) const {return m_key==q.m_key&&m_val==q.m_val;}

        // Swap this with q
        void swap(KeyVal& q) {std::swap(m_key,q.m_key); std::swap(m_val,q.m_val);}

        // Get key
        const std::string& key() const {return m_key;}

        // Set key
        void key(const std::string &k) {m_key=k;}

        // Get value
        const std::string& val() const {return m_val;}

        // Set value
        void val(const std::string &v) {m_val=v;}

        // Output key value pair
        friend std::ostream& operator<<(std::ostream &o, const KeyVal &kv)
            {o<<"<key("<<kv.m_key<<") val("<<kv.m_val<<")> "; return o;}

    private:
        std::string m_key;
        std::string m_val;
    };

    // Define Query as vector of Key Value pairs
    typedef std::vector<KeyVal> Query;

    // Get a reference to the query vector for read only access
    const Query& query() {lazy_parse(); return m_query;}

    // Get a reference to a specific Key Value pair in the query vector for read only access
    const KeyVal& query(size_t i) {
        lazy_parse();
		if (i >= m_query.size()) {
			throw std::out_of_range("Invalid Url query index (" + std::to_string(i) + ")");
		}
        return m_query[i];
    }

    // Get a reference to the query vector for a writable access
    Query& set_query() {lazy_parse(); m_built=false; return m_query;}

    // Get a reference to specific Key Value pair in the query vector for a writable access
    KeyVal& set_query(size_t i) {
        lazy_parse();
		if (i >= m_query.size()) {
			throw std::out_of_range("Invalid Url query index ("+std::to_string(i)+")");
		}
        m_built=false;
        return m_query[i];
    }

    // Set the query vector to the Query vector q
    Url &set_query(const Query &q)
        {lazy_parse(); if (q != m_query) {m_query=q; m_built=false;} return *this;}

    // Append KeyVal kv to the query
    Url &add_query(const KeyVal &kv)
        {lazy_parse(); m_built=false; m_query.push_back(kv); return *this;}

	
	// james: 20160803 no support c++0x.
    // Append key val pair to the query
    Url &add_query(const std::string &key, const std::string &val) {
		lazy_parse(); m_built=false; m_query.emplace_back(key,val); return *this;
	}

	// Append key with empty val to the query
	Url &add_query(const std::string &key) {
		lazy_parse(); m_built = false; m_query.emplace_back(key); return *this;
	}

    // Get the fragment
    const std::string& fragment() {lazy_parse(); return m_fragment;}

    // Set the fragment
    Url &fragment(const std::string& f);

    // Output
    std::ostream& output(std::ostream &o);

    // Output strean operator
    friend std::ostream& operator<<(std::ostream &o, Url &u) {return u.output(o);}

private:
    void assign(const Url &url);
	// james: 20160803 no support c++0x.
    void assign(Url&& url);
    void build_url();
    void lazy_parse() {if (!m_parse) parse_url();}
    void parse_url();

    mutable std::string m_scheme;
    mutable std::string m_user;
    mutable std::string m_host;
    mutable std::string m_port;
	// 20160901 add port.
	unsigned port_;
    mutable std::string m_path;
    mutable Query m_query;
    mutable std::string m_fragment;
	// 20160901 add uri.
	mutable std::string uri_;
    mutable std::string m_url;
    mutable bool m_parse;
    mutable bool m_built;
    mutable std::int8_t m_ip_v;

};