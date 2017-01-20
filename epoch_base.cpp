#include "epoch_base.hpp"

#include <log.hpp>

#include <iomanip>
#include <sstream>

namespace // anonymous
{
constexpr const char* SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr const char* SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr const char* SETTINGS_INTERFACE = SETTINGS_SERVICE;
constexpr const char* PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr const char* METHOD_GET = "Get";
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
    : sdbusplus::server::object::object<EpochTime>(bus, objPath),
      bus(bus)
{
    initialize();
}

void EpochBase::setCurrentTimeMode(const std::string& value)
{
    log<level::INFO>("Time mode is changed",
                     entry("MODE=%s", value.c_str()));
    timeMode = convertToMode(value);
}

void EpochBase::setCurrentTimeOwner(const std::string& value)
{
    log<level::INFO>("Time owner is changed",
                     entry("OWNER=%s", value.c_str()));
    timeOwner = convertToOwner(value);
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

bool EpochBase::setTime(const std::chrono::microseconds& us)
{
    auto method = bus.new_method_call("org.freedesktop.timedate1",
                                      "/org/freedesktop/timedate1",
                                      "org.freedesktop.timedate1",
                                      "SetTime");
    method.append(static_cast<int64_t>(us.count()),
                  false,
                  false);
    auto reply = bus.call(method);
    return static_cast<bool>(reply);
}

std::chrono::microseconds EpochBase::getTime() const
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>
           (now.time_since_epoch());
}

// Accepts the time in microseconds and converts to Human readable format.
std::string EpochBase::convertToStr(const std::chrono::microseconds& us)
{
    using namespace std::chrono;

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
