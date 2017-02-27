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
        log<level::ERR>("Setting BmcTime with NTP mode is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }
    if (timeOwner == Owner::HOST)
    {
        log<level::ERR>("Setting BmcTime with HOST owner is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }

    auto time = std::chrono::microseconds(value);
    setTime(time);

    server::EpochTime::elapsed(value);
    return value;
}

} // namespace time
} // namespace phosphor

