#include "manager.hpp"

#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{

constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_NTP = "SetNTP";
} // namespace

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;

Manager::Manager(sdbusplus::bus::bus& bus, PropertyChangeListner& listener) :
    bus(bus), bmcListener(listener), settings(bus)
{
    using namespace sdbusplus::bus::match::rules;
    settingsMatches.emplace_back(
        bus, propertiesChanged(settings.timeSyncMethod, settings::timeSyncIntf),
        std::bind(std::mem_fn(&Manager::onSettingsChanged), this,
                  std::placeholders::_1));

    // Check the settings daemon to process the new settings
    auto mode = getSetting(settings.timeSyncMethod.c_str(),
                           settings::timeSyncIntf, PROPERTY_TIME_MODE);

    onPropertyChanged(PROPERTY_TIME_MODE, mode);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // Notify listeners
    if (key == PROPERTY_TIME_MODE)
    {
        setCurrentTimeMode(value);
        onTimeModeChanged(value);
    }
}

int Manager::onSettingsChanged(sdbusplus::message::message& msg)
{
    using Interface = std::string;
    using Property = std::string;
    using Value = std::string;
    using Properties = std::map<Property, sdbusplus::message::variant<Value>>;

    Interface interface;
    Properties properties;

    msg.read(interface, properties);

    for (const auto& p : properties)
    {
        onPropertyChanged(
            p.first,
            sdbusplus::message::variant_ns::get<std::string>(p.second));
    }

    return 0;
}

void Manager::updateNtpSetting(const std::string& value)
{
    bool isNtp =
        (value == "xyz.openbmc_project.Time.Synchronization.Method.NTP");
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE, SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE, METHOD_SET_NTP);
    method.append(isNtp, false); // isNtp: 'true/false' means Enable/Disable
                                 // 'false' meaning no policy-kit

    try
    {
        bus.call_noreply(method);
        log<level::INFO>("Updated NTP setting", entry("ENABLED=%d", isNtp));
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Failed to update NTP setting",
                        entry("ERR=%s", ex.what()));
    }
}

bool Manager::setCurrentTimeMode(const std::string& mode)
{
    auto newMode = utils::strToMode(mode);
    if (newMode != timeMode)
    {
        log<level::INFO>("Time mode is changed",
                         entry("MODE=%s", mode.c_str()));
        timeMode = newMode;
        return true;
    }
    else
    {
        return false;
    }
}

void Manager::onTimeModeChanged(const std::string& mode)
{
    bmcListener.onModeChanged(timeMode);

    // When time_mode is updated, update the NTP setting
    updateNtpSetting(mode);
}

std::string Manager::getSetting(const char* path, const char* interface,
                                const char* setting) const
{
    std::string settingManager = utils::getService(bus, path, interface);
    return utils::getProperty<std::string>(bus, settingManager.c_str(), path,
                                           interface, setting);
}

} // namespace time
} // namespace phosphor
