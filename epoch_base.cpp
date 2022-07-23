#include "epoch_base.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

#include <iomanip>
#include <sstream>

namespace // anonymous
{
constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_TIME = "SetTime";
} // namespace

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;
using FailedError = sdbusplus::xyz::openbmc_project::Time::Error::Failed;

EpochBase::EpochBase(sdbusplus::bus_t& bus, const char* objPath) :
    sdbusplus::server::object_t<EpochTime>(bus, objPath), bus(bus)
{}

void EpochBase::onModeChanged(Mode mode)
{
    timeMode = mode;
}

using namespace std::chrono;
bool EpochBase::setTime(const microseconds& usec)
{
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE, SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE, METHOD_SET_TIME);
    method.append(static_cast<int64_t>(usec.count()),
                  false,  // relative
                  false); // user_interaction

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        lg2::error("Error in setting system time: {ERROR}", "ERROR", ex);
        using namespace xyz::openbmc_project::Time;
        elog<FailedError>(Failed::REASON(ex.what()));
    }
    return true;
}

microseconds EpochBase::getTime() const
{
    auto now = system_clock::now();
    return duration_cast<microseconds>(now.time_since_epoch());
}

} // namespace time
} // namespace phosphor
