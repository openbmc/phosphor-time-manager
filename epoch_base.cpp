#include "epoch_base.hpp"
#include "utils.hpp"

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

#include <iomanip>
#include <sstream>

namespace // anonymous
{
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_TIME = "SetTime";
}

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;

EpochBase::EpochBase(sdbusplus::bus::bus& bus,
                     const char* objPath)
    : sdbusplus::server::object::object<EpochTime>(bus, objPath),
      bus(bus)
{
    // Empty
}

void EpochBase::onModeChanged(Mode mode)
{
    timeMode = mode;
}

void EpochBase::onOwnerChanged(Owner owner)
{
    timeOwner = owner;
}

using namespace std::chrono;
void EpochBase::setTime(const microseconds& usec)
{
    std::string timeService = utils::getService(bus,
                                                SYSTEMD_TIME_PATH,
                                                SYSTEMD_TIME_INTERFACE);
    auto method = bus.new_method_call(timeService.c_str(),
                                      SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE,
                                      METHOD_SET_TIME);
    method.append(static_cast<int64_t>(usec.count()),
                  false, // relative
                  false); // user_interaction
    if (!bus.call(method))
    {
        using InternalError = xyz::openbmc_project::Time::EpochTime
                              ::InternalError;
        elog<InternalError>();
    }
}

microseconds EpochBase::getTime() const
{
    auto now = system_clock::now();
    return duration_cast<microseconds>
           (now.time_since_epoch());
}

} // namespace time
} // namespace phosphor
