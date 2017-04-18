#include "manager.hpp"
#include "utils.hpp"

#include <phosphor-logging/log.hpp>

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{
constexpr auto SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr auto SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr auto SETTINGS_INTERFACE = "org.openbmc.settings.Host";

// TODO: Use new settings in xyz.openbmc_project
const auto MATCH_PROPERTY_CHANGE =
    rules::type::signal() +
    rules::member("PropertiesChanged") +
    rules::path("/org/openbmc/settings/host0") +
    rules::interface("org.freedesktop.DBus.Properties");

const auto MATCH_PGOOD_CHANGE =
    rules::type::signal() +
    rules::member("PropertiesChanged") +
    rules::path("/org/openbmc/control/power0") +
    rules::interface("org.freedesktop.DBus.Properties");

constexpr auto POWER_SERVICE = "org.openbmc.control.Power";
constexpr auto POWER_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = POWER_SERVICE;
constexpr auto PGOOD_STR = "pgood";

constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = SYSTEMD_TIME_SERVICE;
constexpr auto METHOD_SET_NTP = "SetNTP";

constexpr auto OBMC_NETWORK_SERVICE = "org.openbmc.NetworkManager";
constexpr auto OBMC_NETWORK_PATH = "/org/openbmc/NetworkManager/Interface";
constexpr auto OBMC_NETWORK_INTERFACE = OBMC_NETWORK_SERVICE;
constexpr auto METHOD_UPDATE_USE_NTP = "UpdateUseNtpField";
}

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;

const std::set<std::string>
Manager::managedProperties = {PROPERTY_TIME_MODE, PROPERTY_TIME_OWNER};

Manager::Manager(sdbusplus::bus::bus& bus)
    : bus(bus),
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this),
      pgoodChangeMatch(bus, MATCH_PGOOD_CHANGE, onPgoodChanged, this)
{
    setCurrentTimeMode(getSettings(PROPERTY_TIME_MODE));
    setCurrentTimeOwner(getSettings(PROPERTY_TIME_OWNER));
    checkHostOn();
    checkDhcpNtp();
}

void Manager::addListener(PropertyChangeListner* listener)
{
    // Notify listener about the initial value
    listener->onModeChanged(timeMode);
    listener->onOwnerChanged(timeOwner);

    listeners.insert(listener);
}

void Manager::checkHostOn()
{
    int pgood = utils::getProperty<int>(bus,
                                        POWER_SERVICE,
                                        POWER_PATH,
                                        POWER_INTERFACE,
                                        PGOOD_STR);
    hostOn = static_cast<bool>(pgood);
}

void Manager::checkDhcpNtp()
{
    std::string useDhcpNtp = utils::getProperty<std::string>(
                                 bus,
                                 SETTINGS_SERVICE,
                                 SETTINGS_PATH,
                                 SETTINGS_INTERFACE,
                                 PROPERTY_DHCP_NTP);
    updateDhcpNtpSetting(useDhcpNtp);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    if (hostOn)
    {
        // If host is on, set the values as requested time mode/owner.
        // And when host becomes off, notify the listners.
        setPropertyAsRequested(key, value);
    }
    else
    {
        // If host is off, notify listners
        if (key == PROPERTY_TIME_MODE)
        {
            setCurrentTimeMode(value);
            onTimeModeChanged(value);
        }
        else if (key == PROPERTY_TIME_OWNER)
        {
            setCurrentTimeOwner(value);
            onTimeOwnerChanged();
        }
    }
}

int Manager::onPropertyChanged(sd_bus_message* msg,
                               void* userData,
                               sd_bus_error* retError)
{
    using properties = std::map < std::string,
          sdbusplus::message::variant<std::string> >;
    auto m = sdbusplus::message::message(msg);
    // message type: sa{sv}as
    std::string ignore;
    properties props;
    m.read(ignore, props);
    auto manager = static_cast<Manager*>(userData);
    for (const auto& item : props)
    {
        if (managedProperties.find(item.first) != managedProperties.end())
        {
            // For managed properties, notify listeners
            manager->onPropertyChanged(
                item.first, item.second.get<std::string>());
        }
        else if (item.first == PROPERTY_DHCP_NTP)
        {
            // For other manager interested properties, handle specifically
            manager->updateDhcpNtpSetting(item.second.get<std::string>());
        }
    }
    return 0;
}

void Manager::setPropertyAsRequested(const std::string& key,
                                     const std::string& value)
{
    if (key == PROPERTY_TIME_MODE)
    {
        setRequestedMode(value);
    }
    else if (key == PROPERTY_TIME_OWNER)
    {
        setRequestedOwner(value);
    }
    else
    {
        // The key shall be already the supported one
        // TODO: use elog API
        assert(false);
    }
}

void Manager::setRequestedMode(const std::string& mode)
{
    requestedMode = mode;
}

void Manager::setRequestedOwner(const std::string& owner)
{
    requestedOwner = owner;
}

void Manager::updateNtpSetting(const std::string& value)
{
    bool isNtp = (value == "NTP");
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE,
                                      SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE,
                                      METHOD_SET_NTP);
    method.append(isNtp, false); // isNtp: 'true/false' means Enable/Disable
                                 // 'false' meaning no policy-kit

    if (bus.call(method))
    {
        log<level::INFO>("Updated NTP setting",
                         entry("ENABLED:%d", isNtp));
    }
    else
    {
        log<level::ERR>("Failed to update NTP setting");
    }
}

void Manager::updateDhcpNtpSetting(const std::string& useDhcpNtp)
{
    auto method = bus.new_method_call(OBMC_NETWORK_SERVICE,
                                      OBMC_NETWORK_PATH,
                                      OBMC_NETWORK_INTERFACE,
                                      METHOD_UPDATE_USE_NTP);
    method.append(useDhcpNtp);

    if (bus.call(method))
    {
        log<level::INFO>("Updated use ntp field",
                         entry("USENTPFIELD:%s", useDhcpNtp.c_str()));
    }
    else
    {
        log<level::ERR>("Failed to update UseNtpField");
    }
}

void Manager::onPgoodChanged(bool pgood)
{
    hostOn = pgood;
    if (hostOn)
    {
        return;
    }
    if (!requestedMode.empty())
    {
        auto modeToSet = requestedMode;
        setRequestedMode({}); // Clear requested mode
        if (setCurrentTimeMode(modeToSet))
        {
            onTimeModeChanged(modeToSet);
        }
    }
    if (!requestedOwner.empty())
    {
        auto ownerToSet = requestedOwner;
        setRequestedOwner({}); // Clear requested owner
        if (setCurrentTimeOwner(ownerToSet))
        {
            onTimeOwnerChanged();
        }
    }
}

int Manager::onPgoodChanged(sd_bus_message* msg,
                            void* userData,
                            sd_bus_error* retError)
{
    using properties = std::map < std::string,
          sdbusplus::message::variant<int> >;
    auto m = sdbusplus::message::message(msg);
    // message type: sa{sv}as
    std::string ignore;
    properties props;
    m.read(ignore, props);
    for (const auto& item : props)
    {
        if (item.first == PGOOD_STR)
        {
            static_cast<Manager*>(userData)
                ->onPgoodChanged(static_cast<bool>(item.second.get<int>()));
        }
    }
    return 0;
}

bool Manager::setCurrentTimeMode(const std::string& mode)
{
    utils::writeData(modeFile, requestedMode);
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

bool Manager::setCurrentTimeOwner(const std::string& owner)
{
    utils::writeData(ownerFile, requestedOwner);
    auto newOwner = utils::strToOwner(owner);
    if (newOwner != timeOwner)
    {
        log<level::INFO>("Time owner is changed",
                         entry("OWNER=%s", owner.c_str()));
        timeOwner = newOwner;
        return true;
    }
    else
    {
        return false;
    }
}

void Manager::onTimeModeChanged(const std::string& mode)
{
    for (const auto listener : listeners)
    {
        listener->onModeChanged(timeMode);
    }
    // When time_mode is updated, update the NTP setting
    updateNtpSetting(mode);
}

void Manager::onTimeOwnerChanged()
{
    for (const auto& listener : listeners)
    {
        listener->onOwnerChanged(timeOwner);
    }
}

std::string Manager::getSettings(const char* value) const
{
    return utils::getProperty<std::string>(
        bus,
        SETTINGS_SERVICE,
        SETTINGS_PATH,
        SETTINGS_INTERFACE,
        value);
}

}
}
