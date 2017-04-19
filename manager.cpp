#include "manager.hpp"
#include "utils.hpp"

#include <phosphor-logging/log.hpp>

namespace // anonymous
{
constexpr auto SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr auto SETTINGS_INTERFACE = "org.openbmc.settings.Host";

constexpr auto MATCH_PROPERTY_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

constexpr auto MATCH_PGOOD_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/control/power0',member='PropertiesChanged'";

constexpr auto POWER_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = "org.openbmc.control.Power";
constexpr auto PGOOD_STR = "pgood";

constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_NTP = "SetNTP";

constexpr auto OBMC_NETWORK_PATH = "/org/openbmc/NetworkManager/Interface";
constexpr auto OBMC_NETWORK_INTERFACE = "org.openbmc.NetworkManager";
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
      pgoodChangeMatch(bus, MATCH_PGOOD_CHANGE, onPgoodChanged, this),
      requestedMode(utils::readData<std::string>(modeFile)),
      requestedOwner(utils::readData<std::string>(ownerFile))
{
    setCurrentTimeMode(getSettings(PROPERTY_TIME_MODE));
    setCurrentTimeOwner(getSettings(PROPERTY_TIME_OWNER));
    checkHostOn();
    initNetworkSetting();
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
    std::string powerService = utils::getService(bus,
                                                 POWER_PATH,
                                                 POWER_INTERFACE);
    int pgood = utils::getProperty<int>(bus,
                                        powerService.c_str(),
                                        POWER_PATH,
                                        POWER_INTERFACE,
                                        PGOOD_STR);
    hostOn = static_cast<bool>(pgood);
}

void Manager::initNetworkSetting()
{
    std::string settingsService = utils::getService(bus,
                                                    SETTINGS_PATH,
                                                    SETTINGS_INTERFACE);
    std::string useDhcpNtp = utils::getProperty<std::string>(
                                 bus,
                                 settingsService.c_str(),
                                 SETTINGS_PATH,
                                 SETTINGS_INTERFACE,
                                 PROPERTY_DHCP_NTP);
    updateNetworkSetting(useDhcpNtp);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // TODO: Check dhcp_ntp

    if (hostOn)
    {
        // If host is on, store the values in persistent storage
        // as requested time mode/owner.
        // And when host becomes off, notify the listners.
        saveProperty(key, value);
    }
    else
    {
        // If host is off, notify listners
        if (key == PROPERTY_TIME_MODE)
        {
            setCurrentTimeMode(value);
            for (const auto listener : listeners)
            {
                listener->onModeChanged(timeMode);
            }
        }
        else if (key == PROPERTY_TIME_OWNER)
        {
            setCurrentTimeOwner(value);
            for (const auto listener : listeners)
            {
                listener->onOwnerChanged(timeOwner);
            }
        }
        if (key == PROPERTY_TIME_MODE)
        {
            // When time_mode is updated, update the NTP setting
            updateNtpSetting(value);
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
    Manager* manager = static_cast<Manager*>(userData);
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
            manager->updateNetworkSetting(item.second.get<std::string>());
        }
    }
    return 0;
}

void Manager::saveProperty(const std::string& key,
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
    utils::writeData(modeFile, requestedMode);
}

void Manager::setRequestedOwner(const std::string& owner)
{
    requestedOwner = owner;
    utils::writeData(ownerFile, requestedOwner);
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

void Manager::updateNetworkSetting(const std::string& useDhcpNtp)
{
    std::string networkService = utils::getService(bus,
                                                   OBMC_NETWORK_PATH,
                                                   OBMC_NETWORK_INTERFACE);
    auto method = bus.new_method_call(networkService.c_str(),
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
        if (setCurrentTimeMode(requestedMode))
        {
            for (const auto& listener : listeners)
            {
                listener->onModeChanged(timeMode);
            }
            updateNtpSetting(requestedMode);
        }
        updateNtpSetting(requestedMode);
        setRequestedMode({});
    }
    if (!requestedOwner.empty())
    {
        if (setCurrentTimeOwner(requestedOwner))
        {
            for (const auto& listener : listeners)
            {
                listener->onOwnerChanged(timeOwner);
            }
        }
        setRequestedOwner({});
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

std::string Manager::getSettings(const char* value) const
{
    std::string settingsService = utils::getService(bus,
                                                    SETTINGS_PATH,
                                                    SETTINGS_INTERFACE);
    return utils::getProperty<std::string>(
        bus,
        settingsService.c_str(),
        SETTINGS_PATH,
        SETTINGS_INTERFACE,
        value);
}

}
}
