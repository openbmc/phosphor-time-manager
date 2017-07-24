#include "manager.hpp"

#include <phosphor-logging/log.hpp>

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{
constexpr auto SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr auto SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr auto SETTINGS_INTERFACE = "org.openbmc.settings.Host";
constexpr auto PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr auto METHOD_GET = "Get";

constexpr auto PROPERTY_TIME_MODE = "time_mode";
constexpr auto PROPERTY_TIME_OWNER = "time_owner";

// TODO: Use new settings in xyz.openbmc_project
const auto MATCH_PROPERTY_CHANGE =
    rules::type::signal() +
    rules::member("PropertiesChanged") +
    rules::path("/org/openbmc/settings/host0") +
    rules::interface("org.freedesktop.DBus.Properties");

}

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;

const std::set<std::string>
Manager::managedProperties = {PROPERTY_TIME_MODE, PROPERTY_TIME_OWNER};

const std::map<std::string, Owner> Manager::ownerMap =
{
    { "BMC", Owner::BMC },
    { "HOST", Owner::HOST },
    { "SPLIT", Owner::SPLIT },
    { "BOTH", Owner::BOTH },
};

Manager::Manager(sdbusplus::bus::bus& bus)
    : bus(bus),
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this)
{
    setCurrentTimeMode(getSettings(PROPERTY_TIME_MODE));
    setCurrentTimeOwner(getSettings(PROPERTY_TIME_OWNER));
}

void Manager::addListener(PropertyChangeListner* listener)
{
    // Notify listener about the initial value
    listener->onModeChanged(timeMode);
    listener->onOwnerChanged(timeOwner);

    listeners.insert(listener);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // TODO: Check pgood
    // If it's off, notify listners;
    // If it's on, hold the values and store in persistent storage.
    // And when pgood turns back to off, notify the listners.

    // TODO: Check dhcp_ntp

    if (key == PROPERTY_TIME_MODE)
    {
        setCurrentTimeMode(value);
        for (const auto& listener : listeners)
        {
            listener->onModeChanged(timeMode);
        }
    }
    else if (key == PROPERTY_TIME_OWNER)
    {
        setCurrentTimeOwner(value);
        for (const auto& listener : listeners)
        {
            listener->onOwnerChanged(timeOwner);
        }
    }
}

int Manager::onPropertyChanged(sd_bus_message* msg,
                               void* userData,
                               sd_bus_error* retError)
{
    using properties = std::map < std::string,
          sdbusplus::message::variant<int, std::string >>;
    auto m = sdbusplus::message::message(msg);
    // message type: sa{sv}as
    std::string ignore;
    properties props;
    m.read(ignore, props);
    for (const auto& item : props)
    {
        if (managedProperties.find(item.first) != managedProperties.end())
        {
            static_cast<Manager*>(userData)
                ->onPropertyChanged(item.first, item.second.get<std::string>());
        }
    }
    return 0;
}


void Manager::setCurrentTimeMode(const std::string& mode)
{
    log<level::INFO>("Time mode is changed",
                     entry("MODE=%s", mode.c_str()));
    timeMode = convertToMode(mode);
}

void Manager::setCurrentTimeOwner(const std::string& owner)
{
    log<level::INFO>("Time owner is changed",
                     entry("OWNER=%s", owner.c_str()));
    timeOwner = convertToOwner(owner);
}

std::string Manager::getSettings(const char* value) const
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
    else
    {
        log<level::ERR>("Failed to get settings");
    }

    return mode.get<std::string>();
}

Mode Manager::convertToMode(const std::string& mode)
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

Owner Manager::convertToOwner(const std::string& owner)
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

}
}
