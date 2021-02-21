#include <date.h>
#include <meta_log.hpp>

#include <iomanip>
#include <iostream>
#include <vector>

namespace metahash::log {
moodycamel::ConcurrentQueue<std::stringstream*>* output_queue = new moodycamel::ConcurrentQueue<std::stringstream*>();
std::thread* cout_printer = new std::thread([]() {
    moodycamel::ConsumerToken ct(*output_queue);

    for (;;) {
        std::vector<std::stringstream*> ssout_list(1000);
        if (auto size = output_queue->try_dequeue_bulk(ct, ssout_list.begin(), 1000)) {
            std::sort(ssout_list.begin(), ssout_list.begin() + size, [](std::stringstream* ss1, std::stringstream* ss2) {
                return ss1->str() < ss2->str();
            });

            for (uint i = 0; i < size; i++) {
                std::cout << ssout_list[i]->rdbuf();
                delete ssout_list[i];

                if (i == size - 1) {
                    std::cout << std::endl;
                } else {
                    std::cout << "\n";
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
});

void print_to_stream_date_and_place(std::stringstream* p_ss, const std::string& file, const std::string& function, const int line)
{
    const auto found = file.find_last_of("/\\");
    const auto filename = (found == std::string::npos) ? file : file.substr(found + 1);

    auto tp = std::chrono::system_clock::now();
    auto dp = date::floor<date::days>(tp);

    auto ymd = date::year_month_day { dp };
    auto time = date::make_time(std::chrono::duration_cast<std::chrono::milliseconds>(tp - dp));

    uint64_t micro = std::chrono::duration_cast<std::chrono::microseconds>(tp - dp).count() % 1000000l;

    (*p_ss) << ymd.year() << "/" << ymd.month() << "/" << ymd.day() << " "
            << std::setfill(' ') << std::setw(2) << time.hours().count() << ":"
            << std::setfill('0') << std::setw(2) << time.minutes().count() << ":"
            << std::setfill('0') << std::setw(2) << time.seconds().count() << ":"
            << std::setfill('0') << std::setw(6) << micro << " "
            << filename << ":" << line << ":" << function << "$\t";
}
}
