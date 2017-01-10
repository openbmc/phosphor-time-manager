#include "bmc_epoch.hpp"

#include <log.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

BmcEpoch::BmcEpoch(sdbusplus::bus::bus& bus,
                   const char* objPath)
    : EpochBase(bus, objPath)
{
    // Empty
}

uint64_t BmcEpoch::elapsed() const
{
    // TODO: get time based on current time mode and owner
    uint64_t time = 0;
    switch (timeOwner)
    {
        case Owner::BMC:
        {
            time = getTime().count();
            break;
        }
        case Owner::HOST:
            break;
        case Owner::SPLIT:
            break;
        case Owner::BOTH:
            break;
    }
    return time;
}

uint64_t BmcEpoch::elapsed(uint64_t value)
{
    // TODO: set time based on current time mode and owner
    auto time = std::chrono::microseconds(value);
    switch (timeOwner)
    {
        case Owner::BMC:
        {
            // TODO: check return value, and throw exception on error
            setTime(time);
            break;
        }
        case Owner::HOST:
            break;
        case Owner::SPLIT:
            break;
        case Owner::BOTH:
            break;
    }
    server::EpochTime::elapsed(value);
    return value;
}

}
}

