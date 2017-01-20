#include "bmc_epoch.hpp"

#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace time
{
namespace server = sdbusplus::xyz::openbmc_project::Time::server;
using namespace phosphor::logging;

BmcEpoch::BmcEpoch(sdbusplus::bus::bus& bus,
                   const char* objPath)
    : EpochBase(bus, objPath)
{
    // Empty
}

uint64_t BmcEpoch::elapsed() const
{
    // It does not needs to check owner when getting time
    return getTime().count();
}

uint64_t BmcEpoch::elapsed(uint64_t value)
{
    // TODO: set time based on current time mode and owner
    auto time = std::chrono::microseconds(value);
    switch (timeOwner)
    {
        case Owner::BMC:
        {
            setTime(time);
            break;
        }
        // TODO: below caese are to be implemented
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

