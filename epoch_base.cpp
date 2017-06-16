#include "epoch_base.hpp"

#include <phosphor-logging/log.hpp>

#include <iomanip>
#include <sstream>

namespace // anonymous
{
constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
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
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE,
                                      SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE,
                                      METHOD_SET_TIME);
    method.append(static_cast<int64_t>(usec.count()),
                  false, // relative
                  false); // user_interaction
    bus.call_noreply(method);
}

microseconds EpochBase::getTime() const
{
    auto now = system_clock::now();
    return duration_cast<microseconds>
           (now.time_since_epoch());
}

} // namespace time
} // namespace phosphor
