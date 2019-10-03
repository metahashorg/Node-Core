#ifndef MHD_MHD_H_
#define MHD_MHD_H_

#include <map>
#include <string>
#include <ctime>
#include <vector>

namespace mh::mhd {

class MHD
{
public:
    enum Expiration
    {
        EXP_NONE,
        EXP_THISHOUR,
        EXP_THISDAY,
        EXP_THISWEEK,
        EXP_THISYEAR,
        EXP_20YEARS,
        EXP_10MIN
    };

    struct Request
    {
        std::string method;
        std::string url;
        std::string version;
        std::string post;

        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> cookies;
        std::map<std::string, std::string> params;
        std::multimap<std::string, std::string> params_multi;

        void clear()
        {
            method.clear();
            url.clear();
            version.clear();
            post.clear();

            headers.clear();
            cookies.clear();
            params.clear();
            params_multi.clear();
        }
    };

    struct Response
    {
        int code = 200;
        std::string data;

        std::map<std::string, std::string> headers;
        std::map<std::string, std::string> cookies;

        void clear()
        {
            code = 200;
            data.clear();
            headers.clear();
            cookies.clear();
        }
    };


    MHD();
    virtual ~MHD() = default;

    bool start(const std::string& path = "");

    virtual bool run(int thread_number, Request& req, Response& resp) = 0;

    static std::string set_cookie(const std::string& key, const std::string& value, time_t ttl,
                                  const std::string& domain);
    static time_t get_expiration(enum Expiration exp);
    static bool parse_qs(const std::string& qs, std::map<std::string, std::string>& params);

protected:
    virtual bool init() = 0;
    virtual void fini(){};
    virtual void idle(){};
    virtual void usr1(){};
    virtual void usr2(){};

    void set_host(const std::string& host);
    void set_port(unsigned int port);
    void set_threads(unsigned int count);
    unsigned int get_threads();
    const std::string& get_config_path();

private:
    std::string config_path;
    std::string host;
    unsigned int port;
    unsigned int threads_count;
};

} // namespace mh::mhd

#endif /* MHD_MHD_H_ */
