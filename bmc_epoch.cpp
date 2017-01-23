#include "bmc_epoch.hpp"

#include <log.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

BmcEpoch::BmcEpoch(sdbusplus::bus::bus& bus,
                   const char* objPath,
                   Manager* manager)
    : EpochBase(bus, objPath, manager)
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
    /*
        Mode  | Owner | Set BMC Time
        ----- | ----- | -------------
        NTP   | BMC   | Not allowed
        NTP   | HOST  | Not allowed
        NTP   | SPLIT | Not allowed
        NTP   | BOTH  | Not allowed
        MANUAL| BMC   | OK
        MANUAL| HOST  | Not allowed
        MANUAL| SPLIT | OK
        MANUAL| BOTH  | OK
    */
    if (timeMode == Mode::NTP)
    {
        log<level::ERR>("Setting BmcTime in NTP mode is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }
    if (timeOwner == Owner::HOST)
    {
        log<level::ERR>("Setting BmcTime in HOST owner is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }
    auto time = std::chrono::microseconds(value);
    if (!setTime(time)) {
        log<level::ERR>("Failed to set BmcTime");
        // TODO: throw InternalError exception
    }
    server::EpochTime::elapsed(value);
    return value;
}

}
}

