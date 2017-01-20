#include "epoch_base.hpp"

#include <log.hpp>

#include <iomanip>
#include <sstream>

namespace // anonymous
{
constexpr auto SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr auto SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr auto SETTINGS_INTERFACE = "org.openbmc.settings.Host";
constexpr auto PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr auto METHOD_GET = "Get";
}

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;

const std::map<std::string, EpochBase::Owner>
EpochBase::ownerMap = {
    { "BMC", EpochBase::Owner::BMC },
    { "HOST", EpochBase::Owner::HOST },
    { "SPLIT", EpochBase::Owner::SPLIT },
    { "BOTH", EpochBase::Owner::BOTH },
};

EpochBase::EpochBase(sdbusplus::bus::bus& bus,
                     const char* objPath)
    : sdbusplus::server::object::object<EpochTime>(bus, objPath, true),
      bus(bus)
{
    initialize();
    // Deferred this until we could get our property correct
    emit_object_added();
}

void EpochBase::setCurrentTimeMode(const std::string& mode)
{
    log<level::INFO>("Time mode is changed",
                     entry("MODE=%s", mode.c_str()));
    timeMode = convertToMode(mode);
}

void EpochBase::setCurrentTimeOwner(const std::string& owner)
{
    log<level::INFO>("Time owner is changed",
                     entry("OWNER=%s", owner.c_str()));
    timeOwner = convertToOwner(owner);
}

void EpochBase::initialize()
{
    setCurrentTimeMode(getSettings("time_mode"));
    setCurrentTimeOwner(getSettings("time_owner"));
    // TODO: subscribe settingsd's property changes callback
}

std::string EpochBase::getSettings(const char* value) const
{
    sdbusplus::message::variant<std::string> mode;
    auto method = bus.new_method_call(SETTINGS_SERVICE,
                                      SETTINGS_PATH,
                                      PROPERTY_INTERFACE,
                                      METHOD_GET);
    method.append(SETTINGS_INTERFACE, value);
    auto reply = bus.call(method);
    if (reply)
    {
        reply.read(mode);
    }

    return mode.get<std::string>();
}

EpochBase::Mode EpochBase::convertToMode(const std::string& mode)
{
    if (mode == "NTP")
    {
        return Mode::NTP;
    }
    else if (mode == "MANUAL")
    {
        return Mode::MANUAL;
    }
    else
    {
        log<level::ERR>("Unrecognized mode",
                        entry("%s", mode.c_str()));
        return Mode::NTP;
    }
}

EpochBase::Owner EpochBase::convertToOwner(const std::string& owner)
{
    auto it = ownerMap.find(owner);
    if (it == ownerMap.end())
    {
        log<level::ERR>("Unrecognized owner",
                        entry("%s", owner.c_str()));
        return Owner::BMC;
    }
    return it->second;
}

using namespace std::chrono;
bool EpochBase::setTime(const microseconds& usec)
{
    auto method = bus.new_method_call("org.freedesktop.timedate1",
                                      "/org/freedesktop/timedate1",
                                      "org.freedesktop.timedate1",
                                      "SetTime");
    method.append(static_cast<int64_t>(usec.count()),
                  false, // relative
                  false); // user_interaction
    auto reply = bus.call(method);
    return static_cast<bool>(reply);
}

microseconds EpochBase::getTime() const
{
    auto now = system_clock::now();
    return duration_cast<microseconds>
           (now.time_since_epoch());
}

// Accepts the time in microseconds and converts to Human readable format.
std::string EpochBase::convertToStr(const microseconds& us)
{

    // Convert this to number of seconds;
    auto timeInSec = duration_cast<seconds>(microseconds(us));
    auto time_T = static_cast<std::time_t>(timeInSec.count());

    std::ostringstream timeFormat;
    timeFormat << std::put_time(std::gmtime(&time_T), "%c %Z");

    auto timeStr = timeFormat.str();
    log<level::DEBUG>("Time str", entry("%s", timeStr.c_str()));
    return timeStr;
}

} // namespace time
} // namespace phosphor
