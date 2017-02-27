#include "host_epoch.hpp"
#include "utils.hpp"

#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;
using namespace std::chrono;

HostEpoch::HostEpoch(sdbusplus::bus::bus& bus,
                     const char* objPath)
    : EpochBase(bus, objPath),
      offset(utils::readData<decltype(offset)::rep>(offsetFile))
{
    // Initialize the diffToSteadyClock
    auto steadyTime = duration_cast<microseconds>(
        steady_clock::now().time_since_epoch());
    diffToSteadyClock = getTime() + offset - steadyTime;
}

uint64_t HostEpoch::elapsed() const
{
    auto ret = getTime();
    if (timeOwner == Owner::SPLIT)
    {
        ret += offset;
    }
    return ret.count();
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

    auto time = microseconds(value);
    if (timeOwner == Owner::SPLIT)
    {
        // Calculate the offset between host and bmc time
        offset = time - getTime();

        // Calculate the diff between host and steady time
        auto steadyTime = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch());
        diffToSteadyClock = time - steadyTime;

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

void HostEpoch::onBmcTimeChanged(microseconds bmcTime)
{
    // If owener is split and BMC time is changed,
    // the offset shall be adjusted
    if (timeOwner == Owner::SPLIT)
    {
        auto steadyTime = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch());
        auto hostTime = steadyTime + diffToSteadyClock;
        offset = hostTime - bmcTime;

        // Store the offset to file
        utils::writeData(offsetFile, offset.count());
    }
}

} // namespace time
} // namespace phosphor

