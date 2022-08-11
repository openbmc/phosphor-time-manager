#include "manager.hpp"

#include "utils.hpp"

#include <assert.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
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

PHOSPHOR_LOG2_USING;

using namespace phosphor::logging;

Manager::Manager(sdbusplus::bus_t& bus) : bus(bus), settings(bus)
{
    using namespace sdbusplus::bus::match::rules;
    settingsMatches.emplace_back(
        bus, propertiesChanged(settings.timeSyncMethod, settings::timeSyncIntf),
        std::bind(std::mem_fn(&Manager::onSettingsChanged), this,
                  std::placeholders::_1));

    // Check the settings daemon to process the new settings
    auto mode = getSetting(settings.timeSyncMethod, settings::timeSyncIntf,
                           PROPERTY_TIME_MODE);

    onPropertyChanged(PROPERTY_TIME_MODE, mode);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    assert(key == PROPERTY_TIME_MODE);

    // Notify listeners
    setCurrentTimeMode(value);
    onTimeModeChanged(value);
}

int Manager::onSettingsChanged(sdbusplus::message_t& msg)
{
    using Interface = std::string;
    using Property = std::string;
    using Value = std::string;
    using Properties = std::map<Property, std::variant<Value>>;

    Interface interface;
    Properties properties;

    msg.read(interface, properties);

    for (const auto& p : properties)
    {
        onPropertyChanged(p.first, std::get<std::string>(p.second));
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
        info("Updated NTP setting: {ENABLED}", "ENABLED", isNtp);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        error("Failed to update NTP setting: {ERROR}", "ERROR", ex);
    }
}

bool Manager::setCurrentTimeMode(const std::string& mode)
{
    auto newMode = utils::strToMode(mode);
    if (newMode != timeMode)
    {
        info("Time mode has been changed to {MODE}", "MODE", newMode);
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
    // When time_mode is updated, update the NTP setting
    updateNtpSetting(mode);
}

std::string Manager::getSetting(const std::string& path,
                                const std::string& interface,
                                const std::string& setting) const
{
    return utils::getProperty<std::string>(bus, path, interface, setting);
}

} // namespace time
} // namespace phosphor
