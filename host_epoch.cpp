#include "host_epoch.hpp"
#include "utils.hpp"

#include <log.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

HostEpoch::HostEpoch(sdbusplus::bus::bus& bus,
                     const char* objPath)
    : EpochBase(bus, objPath),
      offset(utils::readData<decltype(offset)::rep>(offsetFile))
{
    // Empty
}

uint64_t HostEpoch::elapsed() const
{
    // It does not needs to check owner when getting time
    return (getTime() + offset).count();
}

uint64_t HostEpoch::elapsed(uint64_t value)
{
    /*
        Mode  | Owner | Set Host Time
        ----- | ----- | -------------
        NTP   | BMC   | Not allowed
        NTP   | HOST  | Invalid case
        NTP   | SPLIT | OK, and just save offset
        NTP   | BOTH  | OK, and set time to BMC
        MANUAL| BMC   | Not allowed
        MANUAL| HOST  | OK, and set time to BMC
        MANUAL| SPLIT | OK, and just save offset
        MANUAL| BOTH  | OK, and set time to BMC
    */
    if (timeOwner == Owner::BMC)
    {
        log<level::ERR>("Setting HostTime in BMC owner is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }

    if (timeOwner == Owner::HOST && timeMode == Mode::NTP)
    {
        log<level::WARNING>("Ignore time mode NTP when owner is HOST");
        return 0;
    }

    auto time = std::chrono::microseconds(value);
    if (timeOwner == Owner::SPLIT)
    {
        // Just save offset
        offset = time - getTime();
        // Store the offset to file
        utils::writeData(offsetFile, offset.count());
    }
    else
    {
        // Set time to BMC
        setTime(time);
    }

    server::EpochTime::elapsed(value);
    return value;
}

} // namespace time
} // namespace phosphor

