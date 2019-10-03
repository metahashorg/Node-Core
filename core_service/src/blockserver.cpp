#include "blockserver.h"
#include <thread>

BLOCK_SERVER::BLOCK_SERVER(int _port, const std::function<std::string(const std::string&, const std::string&)>& func)
    : processor(func)
{
    //        set_host(const string& host);
    set_port(_port);
    set_threads(std::thread::hardware_concurrency());
}

BLOCK_SERVER::~BLOCK_SERVER() = default;

bool BLOCK_SERVER::run(int /*thread_number*/, mh::mhd::MHD::Request& mhd_req, mh::mhd::MHD::Response& mhd_resp)
{
    std::string resp = processor(mhd_req.post, mhd_req.url);

    if (resp.empty()) {
        mhd_resp.data = "ok";
    } else {
        mhd_resp.data = std::move(resp);
    }
    return true;
}

bool BLOCK_SERVER::init()
{
    return true;
}
