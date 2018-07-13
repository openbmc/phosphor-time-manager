#include "host_epoch.hpp"
#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;
using namespace std::chrono;

using NotAllowedError =
    sdbusplus::xyz::openbmc_project::Time::Error::NotAllowed;

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
    if (timeOwner == Owner::Split)
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
        NTP   | HOST  | Not allowed
        NTP   | SPLIT | OK, and just save offset
        NTP   | BOTH  | Not allowed
        MANUAL| BMC   | Not allowed
        MANUAL| HOST  | OK, and set time to BMC
        MANUAL| SPLIT | OK, and just save offset
        MANUAL| BOTH  | OK, and set time to BMC
    */
    if (timeOwner == Owner::BMC ||
        (timeMode == Mode::NTP
         && (timeOwner == Owner::Host || timeOwner == Owner::Both)))
    {
        using namespace xyz::openbmc_project::Time;
        elog<NotAllowedError>(
            NotAllowed::OWNER(utils::ownerToStr(timeOwner).c_str()),
            NotAllowed::SYNC_METHOD(utils::modeToStr(timeMode).c_str()),
            NotAllowed::REASON("Setting HostTime is not allowed"));
    }

    auto time = microseconds(value);
    if (timeOwner == Owner::Split)
    {
        // Calculate the offset between host and bmc time
        offset = time - getTime();
        saveOffset();

        // Calculate the diff between host and steady time
        auto steadyTime = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch());
        diffToSteadyClock = time - steadyTime;
    }
    else
    {
        // Set time to BMC
        setTime(time);
    }

    server::EpochTime::elapsed(value);
    return value;
}

void HostEpoch::onOwnerChanged(Owner owner)
{
    // If timeOwner is changed to SPLIT, the offset shall be preserved
    // Otherwise it shall be cleared;
    timeOwner = owner;
    if (timeOwner != Owner::Split)
    {
        offset = microseconds(0);
        saveOffset();
    }
    else
    {
        // In SPLIT, need to re-calculate the diff between
        // host and steady time
        auto steadyTime = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch());
        diffToSteadyClock = getTime() - steadyTime;
    }
}

void HostEpoch::saveOffset()
{
    // Store the offset to file
    utils::writeData(offsetFile, offset.count());
}

void HostEpoch::onBmcTimeChanged(const microseconds& bmcTime)
{
    // If owner is split and BMC time is changed,
    // the offset shall be adjusted
    if (timeOwner == Owner::Split)
    {
        auto steadyTime = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch());
        auto hostTime = steadyTime + diffToSteadyClock;
        offset = hostTime - bmcTime;

        saveOffset();
    }
}

} // namespace time
} // namespace phosphor

