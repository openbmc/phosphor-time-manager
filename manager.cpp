#include "manager.hpp"
#include "utils.hpp"

#include <phosphor-logging/log.hpp>

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{
constexpr auto SETTINGS_SERVICE = "org.openbmc.settings.Host";
constexpr auto SETTINGS_PATH = "/org/openbmc/settings/host0";
constexpr auto SETTINGS_INTERFACE = "org.openbmc.settings.Host";
constexpr auto PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr auto METHOD_GET = "Get";

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

// TODO: consider put the get properties related functions into a common place
constexpr auto POWER_SERVICE = "org.openbmc.control.Power";
constexpr auto POWER_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = POWER_SERVICE;
constexpr auto PGOOD_STR = "pgood";
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
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this),
      pgoodChangeMatch(bus, MATCH_PGOOD_CHANGE, onPgoodChanged, this)
{
    setCurrentTimeMode(getSettings(PROPERTY_TIME_MODE));
    setCurrentTimeOwner(getSettings(PROPERTY_TIME_OWNER));
    checkHostOn();
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
    sdbusplus::message::variant<int> pgood = 0;
    auto method = bus.new_method_call(POWER_SERVICE,
                                      POWER_PATH,
                                      PROPERTY_INTERFACE,
                                      METHOD_GET);
    method.append(PROPERTY_INTERFACE, PGOOD_STR);
    auto reply = bus.call(method);
    if (reply)
    {
        reply.read(pgood);
    }

    hostOn = static_cast<bool>(pgood.get<int>());
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // TODO: Check dhcp_ntp

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
    for (const auto& item : props)
    {
        if (managedProperties.find(item.first) != managedProperties.end())
        {
            static_cast<Manager*>(userData)->onPropertyChanged(
                item.first, item.second.get<std::string>());
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
        setCurrentTimeMode(modeToSet);
        for (const auto& listener : listeners)
        {
            listener->onModeChanged(timeMode);
        }
    }
    if (!requestedOwner.empty())
    {
        auto ownerToSet = requestedOwner;
        setRequestedOwner({}); // Clear requested owner
        setCurrentTimeOwner(ownerToSet);
        for (const auto& listener : listeners)
        {
            listener->onOwnerChanged(timeOwner);
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

void Manager::setCurrentTimeMode(const std::string& mode)
{
    log<level::INFO>("Time mode is changed",
                     entry("MODE=%s", mode.c_str()));
    timeMode = convertToMode(mode);
    utils::writeData(modeFile, requestedMode);
}

void Manager::setCurrentTimeOwner(const std::string& owner)
{
    log<level::INFO>("Time owner is changed",
                     entry("OWNER=%s", owner.c_str()));
    timeOwner = convertToOwner(owner);
    utils::writeData(ownerFile, requestedOwner);
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
