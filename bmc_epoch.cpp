#include "bmc_epoch.hpp"

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <sys/timerfd.h>
#include <unistd.h>


// Need to do this since its not exported outside of the kernel.
// Refer : https://gist.github.com/lethean/446cea944b7441228298
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

// Needed to make sure timerfd does not misfire even though we set CANCEL_ON_SET
#define TIME_T_MAX (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

namespace phosphor
{
namespace time
{
namespace server = sdbusplus::xyz::openbmc_project::Time::server;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

BmcEpoch::BmcEpoch(sdbusplus::bus::bus& bus,
                   const char* objPath)
    : EpochBase(bus, objPath),
      bus(bus)
{
    initialize();
}

void BmcEpoch::initialize()
{
    using InternalFailure = sdbusplus::xyz::openbmc_project::Common::
                                Error::InternalFailure;

    // Subscribe time change event
    // Choose the MAX time that is possible to avoid mis fires.
    constexpr itimerspec maxTime = {
        {0, 0}, // it_interval
        {TIME_T_MAX, 0}, //it_value
    };

    timeFd = timerfd_create(CLOCK_REALTIME, 0);
    if (timeFd == -1)
    {
        log<level::ERR>("Failed to create timerfd",
                        entry("ERRNO=%d", errno),
                        entry("ERR=%s", strerror(errno)));
        elog<InternalFailure>();
    }

    auto r = timerfd_settime(timeFd,
                             TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET,
                             &maxTime,
                             nullptr);
    if (r != 0)
    {
        log<level::ERR>("Failed to set timerfd",
                        entry("ERRNO=%d", errno),
                        entry("ERR=%s", strerror(errno)));
        elog<InternalFailure>();
    }

    sd_event_source* es;
    r = sd_event_add_io(bus.get_event(), &es,
                        timeFd, EPOLLIN, onTimeChange, this);
    if (r < 0)
    {
        log<level::ERR>("Failed to add event",
                        entry("ERRNO=%d", -r),
                        entry("ERR=%s", strerror(-r)));
        elog<InternalFailure>();
    }
    timeChangeEventSource.reset(es);
}

BmcEpoch::~BmcEpoch()
{
    close(timeFd);
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
        NTP   | BMC   | Fail to set
        NTP   | HOST  | Not allowed
        NTP   | SPLIT | Fail to set
        NTP   | BOTH  | Fail to set
        MANUAL| BMC   | OK
        MANUAL| HOST  | Not allowed
        MANUAL| SPLIT | OK
        MANUAL| BOTH  | OK
    */
    if (timeOwner == Owner::Host)
    {
        log<level::ERR>("Setting BmcTime with HOST owner is not allowed");
        elog<InsufficientPermission>();
        return 0;
    }

    auto time = microseconds(value);
    if (setTime(time))
    {
        notifyBmcTimeChange(time);
    }

    server::EpochTime::elapsed(value);
    return value;
}

void BmcEpoch::setBmcTimeChangeListener(BmcTimeChangeListener* listener)
{
    timeChangeListener = listener;
}

void BmcEpoch::notifyBmcTimeChange(const microseconds& time)
{
    // Notify listener if it exists
    if (timeChangeListener)
    {
        timeChangeListener->onBmcTimeChanged(time);
    }
}

int BmcEpoch::onTimeChange(sd_event_source* es, int fd,
                           uint32_t /* revents */, void* userdata)
{
    auto bmcEpoch = static_cast<BmcEpoch*>(userdata);

    std::array<char, 64> time {};

    // We are not interested in the data here.
    // So read until there is no new data here in the FD
    while (read(fd, time.data(), time.max_size()) > 0);

    log<level::INFO>("BMC system time is changed");
    bmcEpoch->notifyBmcTimeChange(bmcEpoch->getTime());

    return 0;
}

} // namespace time
} // namespace phosphor

