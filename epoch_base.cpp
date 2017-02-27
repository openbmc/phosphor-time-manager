#include "epoch_base.hpp"

#include <phosphor-logging/log.hpp>

#include <iomanip>
#include <sstream>

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
    auto method = bus.new_method_call("org.freedesktop.timedate1",
                                      "/org/freedesktop/timedate1",
                                      "org.freedesktop.timedate1",
                                      "SetTime");
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
