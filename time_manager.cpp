#include "time_manager.hpp"

namespace // anonymous
{
constexpr auto MATCH_PROPERTY_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";
constexpr auto MATCH_PGOOD_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/control/power0',member='PropertiesChanged'";

// TODO: consider put the get properties related functions into a common place
constexpr auto POWER_SERVICE = "org.openbmc.control.Power";
constexpr auto POWER_PATH = "/org/openbmc/control/power0";
constexpr auto POWER_INTERFACE = POWER_SERVICE;
constexpr auto PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr auto METHOD_GET = "Get";
constexpr auto PGOOD_STR = "pgood";
}

namespace phosphor
{
namespace time
{

const std::set<std::string>
Manager::managedProperties = {"time_mode", "time_owner"};

Manager::Manager(sdbusplus::bus::bus& bus)
    : bus(bus),
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this),
      pgoodChangeMatch(bus, MATCH_PGOOD_CHANGE, onPgoodChanged, this)
{
    initPgood();
}

void Manager::addListener(PropertyChangeListner* listener)
{
    listeners.insert(listener);
}

void Manager::initPgood()
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

    isHostOn = static_cast<bool>(pgood.get<int>());
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // TODO: Check pgood
    // If it's off, notify listners;
    // If it's on, hold the values and store in persistent storage
    // as requested time mode/owner.
    // And when pgood turns back to off, notify the listners.
    for (const auto listener : listeners)
    {
        listener->onPropertyChange(key, value);
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
    for (const auto item : props)
    {
        if (managedProperties.find(item.first) != managedProperties.end())
        {
            static_cast<Manager*>(userData)
                ->onPropertyChanged(item.first, item.second.get<std::string>());
        }
    }
    return 0;
}

void Manager::onPgoodChanged(bool pgood)
{
    isHostOn = pgood;
    // TODO: if host is off, check requested time_mode/owner:
    // and notify the listeners if any.
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
