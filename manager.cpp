#include "manager.hpp"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cassert>

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{

constexpr auto systemdTimeService = "org.freedesktop.timedate1";
constexpr auto systemdTimePath = "/org/freedesktop/timedate1";
constexpr auto systemdTimeInterface = "org.freedesktop.timedate1";
constexpr auto methodSetNtp = "SetNTP";
} // namespace

namespace phosphor
{
namespace time
{

PHOSPHOR_LOG2_USING;

Manager::Manager(sdbusplus::bus_t& bus) : bus(bus), settings(bus)
{
    using namespace sdbusplus::bus::match::rules;
    settingsMatches.emplace_back(
        bus, propertiesChanged(settings.timeSyncMethod, settings::timeSyncIntf),
        [&](sdbusplus::message_t& m) { onSettingsChanged(m); });

    // Check the settings daemon to process the new settings
    auto mode = getSetting(settings.timeSyncMethod.c_str(),
                           settings::timeSyncIntf, propertyTimeMode);

    onPropertyChanged(propertyTimeMode, mode);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    assert(key == propertyTimeMode);

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
    try
    {
        bool isNtp =
            (value == "xyz.openbmc_project.Time.Synchronization.Method.NTP");
        auto method = bus.new_method_call(systemdTimeService, systemdTimePath,
                                          systemdTimeInterface, methodSetNtp);
        method.append(isNtp, false); // isNtp: 'true/false' means Enable/Disable
                                     // 'false' meaning no policy-kit

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
    try
    {
        auto newMode = utils::strToMode(mode);
        if (newMode != timeMode)
        {
            info("Time mode has been changed to {MODE}", "MODE", newMode);
            timeMode = newMode;
            return true;
        }
    }
    catch (const sdbusplus::exception_t& ex)
    {
        error("Failed to convert mode from string: {ERROR}", "ERROR", ex);
    }

    return false;
}

void Manager::onTimeModeChanged(const std::string& mode)
{
    // When time_mode is updated, update the NTP setting
    updateNtpSetting(mode);
}

std::string Manager::getSetting(const char* path, const char* interface,
                                const char* setting) const
{
    try
    {
        std::string settingManager = utils::getService(bus, path, interface);
        return utils::getProperty<std::string>(bus, settingManager.c_str(),
                                               path, interface, setting);
    }
    catch (const std::exception& ex)
    {
        error(
            "Failed to get property: {ERROR}, path: {PATH}, interface: {INTERFACE}, name: {NAME}",
            "ERROR", ex, "PATH", path, "INTERFACE", interface, "NAME", setting);
        return {};
    }
}

} // namespace time
} // namespace phosphor
