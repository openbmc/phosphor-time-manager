#include "bmc_epoch.hpp"

#include "utils.hpp"

#include <sys/timerfd.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <string>

// Need to do this since its not exported outside of the kernel.
// Refer : https://gist.github.com/lethean/446cea944b7441228298
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

namespace phosphor
{
namespace time
{
namespace // anonymous
{
constexpr auto systemdTimeService = "org.freedesktop.timedate1";
constexpr auto systemdTimePath = "/org/freedesktop/timedate1";
constexpr auto systemdTimeInterface = "org.freedesktop.timedate1";
constexpr auto methodSetTime = "SetTime";
} // namespace

PHOSPHOR_LOG2_USING;

namespace server = sdbusplus::xyz::openbmc_project::Time::server;
using namespace phosphor::logging;
using FailedError = sdbusplus::xyz::openbmc_project::Time::Error::Failed;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::system_clock;

/** @brief Convert microseconds-since-epoch to a human-readable UTC string.
 *
 *  @param[in] us - Microseconds since the Unix epoch
 *  @return       - String in the human readable form
 */
std::string toUtcString(std::chrono::microseconds us)
{
    const auto secondsSinceEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(us).count();
    const std::time_t timeT = static_cast<std::time_t>(secondsSinceEpoch);
    std::tm utc{};
    gmtime_r(&timeT, &utc);
    std::array<char, 32> buf{};
    std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S UTC", &utc);
    return {buf.data()};
}

void BmcEpoch::initialize()
{
    using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

    // Subscribe time change event
    // Choose the MAX time that is possible to avoid misfires.
    constexpr itimerspec maxTime = {
        {0, 0},                                     // it_interval
        {system_clock::duration::max().count(), 0}, // it_value
    };

    timeFd = timerfd_create(CLOCK_REALTIME, 0);
    if (timeFd == -1)
    {
        error("Failed to create timerfd: {ERRNO}", "ERRNO", errno);
        elog<InternalFailure>();
    }

    auto r = timerfd_settime(
        timeFd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &maxTime, nullptr);
    if (r != 0)
    {
        error("Failed to set timerfd: {ERRNO}", "ERRNO", errno);
        elog<InternalFailure>();
    }

    sd_event_source* es = nullptr;
    r = sd_event_add_io(bus.get_event(), &es, timeFd, EPOLLIN, onTimeChange,
                        this);
    if (r < 0)
    {
        error("Failed to add event: {ERRNO}", "ERRNO", errno);
        elog<InternalFailure>();
    }
    timeChangeEventSource.reset(es);

    previousEpoch = getTime();
}

BmcEpoch::~BmcEpoch()
{
    close(timeFd);
}

uint64_t BmcEpoch::elapsed() const
{
    return getTime().count();
}

uint64_t BmcEpoch::elapsed(uint64_t value)
{
    /*
        Mode  | Set BMC Time
        ----- | -------------
        NTP   | Fail to set
        MANUAL| OK
    */
    auto time = microseconds(value);
    setTime(time);

    server::EpochTime::elapsed(value);
    return value;
}

int BmcEpoch::onTimeChange(sd_event_source* /* es */, int fd,
                           uint32_t /* revents */, void* userdata)
{
    std::array<char, 64> time{};

    // We are not interested in the data here.
    // So read until there is no new data here in the FD
    while (read(fd, time.data(), time.max_size()) > 0)
    {
        ;
    }

    auto* self = static_cast<BmcEpoch*>(userdata);
    if (self != nullptr)
    {
        const auto newTime = getTime();
        const auto prevTime = self->previousEpoch;
        const auto deltaSeconds =
            duration_cast<std::chrono::seconds>(newTime - prevTime).count();

        info("TIME_CHANGE_AUDIT: system clock changed | "
             "previous={PREVIOUS_TIME} "
             "new={NEW_TIME} "
             "delta_seconds={DELTA_SECONDS}",
             "mode={MODE}", "PREVIOUS_TIME", toUtcString(prevTime), "NEW_TIME",
             toUtcString(newTime), "DELTA_SECONDS", deltaSeconds, "MODE",
             utils::modeToStr(self->manager.getTimeMode()));

        self->previousEpoch = newTime;

        constexpr itimerspec maxTime = {
            {0, 0},
            {std::numeric_limits<time_t>::max(), 0},
        };
        timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET,
                        &maxTime, nullptr);
    }

    return 0;
}

void BmcEpoch::onModeChanged(Mode mode)
{
    manager.setTimeMode(mode);
}

bool BmcEpoch::setTime(const microseconds& usec)
{
    auto method = bus.new_method_call(systemdTimeService, systemdTimePath,
                                      systemdTimeInterface, methodSetTime);
    method.append(static_cast<int64_t>(usec.count()),
                  false,  // relative
                  false); // user_interaction

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        error("Error in setting system time: {ERROR}", "ERROR", ex);
        using namespace xyz::openbmc_project::Time;
        elog<FailedError>(Failed::REASON(ex.what()));
    }

    return true;
}

microseconds BmcEpoch::getTime()
{
    auto now = system_clock::now();
    return duration_cast<microseconds>(now.time_since_epoch());
}

} // namespace time
} // namespace phosphor
