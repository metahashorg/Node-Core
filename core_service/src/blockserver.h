#ifndef BLOCKSERVER_H
#define BLOCKSERVER_H

#include <MHD.h>
#include <functional>

class BLOCK_SERVER : public mh::mhd::MHD {
private:
    std::function<std::string(const std::string_view, const std::string_view, const std::string_view, const std::string_view)> processor;

public:
    BLOCK_SERVER(int _port, const std::function<std::string(const std::string_view, const std::string_view, const std::string_view, const std::string_view)>& func);

    ~BLOCK_SERVER() override;

    bool run(int thread_number, Request& mhd_req, Response& mhd_resp) override;

protected:
    bool init() override;
};

#endif // BLOCKSERVER_H
