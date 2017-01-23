#include "log.hpp"
#include "time_manager.hpp"
#include "utils.hpp"

namespace // anonymous
{
constexpr auto MATCH_PROPERTY_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";
constexpr auto MATCH_PGOOD_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/control/power0',member='PropertiesChanged'";

constexpr auto POWER_SERVICE = "org.openbmc.control.Power";
constexpr auto POWER_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = POWER_SERVICE;
constexpr auto PGOOD_STR = "pgood";

constexpr const char* SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr const char* SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr const char* SETTINGS_INTERFACE = SETTINGS_SERVICE;
constexpr auto PROPERTY_MODE = "time_mode";
constexpr auto PROPERTY_OWNER = "time_owner";
constexpr auto PROPERTY_DHCP_NTP = "use_dhcp_ntp";

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
Manager::managedProperties = {PROPERTY_MODE, PROPERTY_OWNER};

Manager::Manager(sdbusplus::bus::bus& bus)
    : bus(bus),
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this),
      pgoodChangeMatch(bus, MATCH_PGOOD_CHANGE, onPgoodChanged, this),
      requestedMode(utils::readData<std::string>(modeFile)),
      requestedOwner(utils::readData<std::string>(ownerFile))
{
    initPgood();
    initNetworkSetting();
}

void Manager::addListener(PropertyChangeListner* listener)
{
    listeners.insert(listener);
}

void Manager::initPgood()
{
    int pgood = utils::getProperty<int>(bus,
                                        POWER_SERVICE,
                                        POWER_PATH,
                                        POWER_INTERFACE,
                                        PGOOD_STR);
    isHostOn = static_cast<bool>(pgood);
}

void Manager::initNetworkSetting()
{
    std::string useDhcpNtp = utils::getProperty<std::string>(
                                 bus,
                                 SETTINGS_SERVICE,
                                 SETTINGS_PATH,
                                 SETTINGS_INTERFACE,
                                 PROPERTY_DHCP_NTP);
    updateNetworkSetting(useDhcpNtp);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    if (isHostOn)
    {
        // If host is on, store the values in persistent storage
        // as requested time mode/owner.
        // And when host becomes off, notify the listners.
        saveProperty(key, value);
    }
    else
    {
        // If host is off, notify listners
        for (const auto& listener : listeners)
        {
            listener->onPropertyChange(key, value);
        }
        if (key == PROPERTY_MODE)
        {
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
    if (key == PROPERTY_MODE)
    {
        setRequestedMode(value);
    }
    else if (key == PROPERTY_OWNER)
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
    int isNtp = (value == "NTP");
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE,
                                      SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE,
                                      METHOD_SET_NTP);
    method.append(isNtp, 0); // isNtp: '1/0' means Enable/Disable
                             // '0' meaning no policy-kit

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
    isHostOn = pgood;
    if (isHostOn)
    {
        return;
    }
    if (!requestedMode.empty())
    {
        for (const auto& listener : listeners)
        {
            listener->onPropertyChange(PROPERTY_MODE, requestedMode);
        }
        updateNtpSetting(requestedMode);
        setRequestedMode("");
    }
    if (!requestedOwner.empty())
    {
        for (const auto& listener : listeners)
        {
            listener->onPropertyChange(PROPERTY_OWNER, requestedOwner);
        }
        setRequestedOwner("");
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
    for (const auto item : props)
    {
        if (item.first == PGOOD_STR)
        {
            static_cast<Manager*>(userData)
                ->onPgoodChanged(static_cast<bool>(item.second.get<int>()));
        }
    }
    return 0;
}

}
}
