#include "blockserver.h"
#include <thread>

BLOCK_SERVER::BLOCK_SERVER(int _port, const std::function<std::string(const std::string_view, const std::string_view, const std::string_view, const std::string_view)>& func)
    : processor(func)
{
    //        set_host(const string& host);
    set_port(_port);
    set_threads(std::thread::hardware_concurrency());
}

BLOCK_SERVER::~BLOCK_SERVER() = default;

bool BLOCK_SERVER::run(int /*thread_number*/, mh::mhd::MHD::Request& mhd_req, mh::mhd::MHD::Response& mhd_resp)
{
    std::string path = mhd_req.url;
    path.erase(std::remove(path.begin(), path.end(), '/'), path.end());

    std::string resp = processor(mhd_req.post, path, mhd_req.headers["Sign"], mhd_req.headers["PublicKey"]);

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
