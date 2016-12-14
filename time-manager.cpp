#define _XOPEN_SOURCE
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <array>
#include <unistd.h>
#include <assert.h>
#include <sys/timerfd.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include "time-manager.hpp"
#include "xyz/openbmc_project/Time/EpochTime/server.hpp"

namespace phosphor
{
namespace time
{

// Common routine for Get Time operations
std::chrono::microseconds EpochTime::getBaseTime() const
{
    auto currBmcTime = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>
           (currBmcTime.time_since_epoch());
}

// api for get time call.
uint64_t EpochTime::elapsed() const
{
//  std::cout << "Request to get elapsed time: ";

    std::cerr << objpath << "Epoch-time"<< std::endl;
    auto timeInUsec = getBaseTime();
    return (uint64_t)timeInUsec.count();
}

EpochTime::EpochTime(sdbusplus::bus::bus &&bus,
                 const char* busname,
                 const char* obj) :
    detail::ServerObject<detail::EpochTimeIface>(bus, obj),
    _bus(std::move(bus)),
    _manager(sdbusplus::server::manager::manager(_bus, obj))
{
    _bus.request_name(busname);
    objpath = obj;
}

void EpochTime::run() noexcept
{
    while(true)
    {
        try
        {
            _bus.process_discard();
            _bus.wait();
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

} // namespace time
} // namespace phosphor

