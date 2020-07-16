#include "core_service.h"

#include <meta_log.hpp>
#include <version.h>

void print_config_file_params_and_exit()
{
    static const std::string version = std::string(VESION_MAJOR) + "." + std::string(VESION_MINOR) + "." + std::string(GIT_COUNT) + "." + std::string(GIT_COMMIT_HASH);
    DEBUG_COUT(version);
    DEBUG_COUT("Example configuration");
    DEBUG_COUT(R"(
{
  "network": "net-main",
  "hostname": "1.2.3.4",
  "port": 9999,
  "key": "0x307402abcdef....",
  "path": "/data/metahash",
  "hash": "85e6c78616632e4fba97efb1dfb403834fe909bc34e3c7efa836ff2ea974ba9b",
  "cores": [
    {
      "network": "0x00fca67778165988703a302c1dfc34fd6036e209a20666969e",
      "host": "31.172.81.114",
      "port": 9999
    }
  ]
}
               )");

    std::this_thread::sleep_for(std::chrono::seconds(2));
    exit(1);
}
