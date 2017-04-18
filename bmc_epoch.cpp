#include "bmc_epoch.hpp"
#include "utils.hpp"

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
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
    using NotAllowed = xyz::openbmc_project::Time::EpochTime::NotAllowed;
    using CURRENT_MODE = NotAllowed::CURRENT_MODE;
    using CURRENT_OWNER = NotAllowed::CURRENT_OWNER;
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
    if (timeMode == Mode::NTP || timeOwner == Owner::HOST)
    {
        elog<NotAllowed>(CURRENT_MODE(utils::modeToStr(timeMode)),
                         CURRENT_OWNER(utils::ownerToStr(timeOwner)));
    }

    auto time = std::chrono::microseconds(value);
    setTime(time);

    server::EpochTime::elapsed(value);
    return value;
}

} // namespace time
} // namespace phosphor

