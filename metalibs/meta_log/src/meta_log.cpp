#include <meta_log.hpp>

#include <ctime>
#include <date.h>
#include <iomanip>
#include <iostream>

#include "date.h"

moodycamel::ConcurrentQueue<std::stringstream*>* __output_queue = new moodycamel::ConcurrentQueue<std::stringstream*>();
std::thread* __cout_printer = new std::thread([]() {
    moodycamel::ConsumerToken ct(*__output_queue);

    while (true) {
        std::stringstream* p_ssout;
        if (__output_queue->try_dequeue(ct, p_ssout)) {

            std::cout << p_ssout->rdbuf() << std::endl;

            delete p_ssout;
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

    auto ymd = date::year_month_day{ dp };
    auto time = date::make_time(std::chrono::duration_cast<std::chrono::milliseconds>(tp - dp));

    (*p_ss) << ymd.year() << "/" << ymd.month() << "/" << ymd.day() << " "
            << std::setfill(' ') << std::setw(2) << time.hours().count() << ":"
            << std::setfill('0') << std::setw(2) << time.minutes().count() << ":"
            << std::setfill('0') << std::setw(2) << time.seconds().count() << ":"
            << std::setfill('0') << std::setw(3) << time.subseconds().count() << " "
            << filename << ":" << line << ":" << function << "$\t";
}
