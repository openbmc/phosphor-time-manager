#include "epoch_base.hpp"

#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

#include <iomanip>
#include <optional>
#include <sstream>

namespace // anonymous
{
constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_TIME = "SetTime";
constexpr auto PROPERTY_NTP = "NTP";
} // namespace

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;
using FailedError = sdbusplus::xyz::openbmc_project::Time::Error::Failed;

EpochBase::EpochBase(sdbusplus::bus_t& bus, const char* objPath,
                     Manager& manager) :
    sdbusplus::server::object_t<EpochTime>(bus, objPath),
    bus(bus), manager(manager)
{}

void EpochBase::onModeChanged(Mode mode)
{
    manager.setTimeMode(mode);
}

using namespace std::chrono;
bool EpochBase::setTime(const microseconds& usec)
{
    if (manager.getTimeMode() == Mode::Manual)
    {
        // When the NTP service is disbale through Redfish or other methods, NTP
        // is being stopped but not completed. At this time, there will be an
        // error in setting the time. We don't want to throw this error to the
        // upper layer. We need to wait for the NTP service to stop and complete
        // the time before setting the time. , the default wait time is 20
        // seconds
        // issue: openbmc/openbmc#3459
        for (int i = 0; i < 20; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            try
            {
                auto ntp = getNTP();
                if (ntp && *ntp == false)
                {
                    break;
                }
            }
            catch (const std::exception& e)
            {
                lg2::error("reading NTP state failed: {ERROR}", "ERROR", e);
                using namespace xyz::openbmc_project::Time;
                elog<FailedError>(Failed::REASON(e.what()));
            }
        }
    }

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

std::optional<bool> EpochBase::getNTP() const
{
    try
    {
        return utils::getProperty<bool>(bus, SYSTEMD_TIME_SERVICE,
                                        SYSTEMD_TIME_PATH,
                                        SYSTEMD_TIME_INTERFACE, PROPERTY_NTP);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        lg2::error("Failed to get NTP: {ERROR}", "ERROR", ex);
        return std::nullopt;
    }
}

} // namespace time
} // namespace phosphor
