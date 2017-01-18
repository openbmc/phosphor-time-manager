#include "time_manager.hpp"

namespace // anonymous
{
constexpr auto MATCH_PROPERTY_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

}

namespace phosphor
{
namespace time
{

const std::set<std::string>
Manager::managedProperties = {"time_mode", "time_owner"};

Manager::Manager(sdbusplus::bus::bus& bus)
    : bus(bus),
      propertyChangeMatch(bus, MATCH_PROPERTY_CHANGE, onPropertyChanged, this)
{
}

void Manager::addListener(PropertyChangeListner* listener)
{
    listeners.insert(listener);
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    // TODO: Check pgood
    // If it's off, notify listners;
    // If it's on, hold the values and store in persistent storage.
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
          sdbusplus::message::variant<int, std::string >>;
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

}
}
