#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/match.hpp>

#include <set>
#include <string>

namespace phosphor
{
namespace time
{

class PropertyChangeListner
{
    public:
        virtual ~PropertyChangeListner() {}
        virtual void onPropertyChange(const std::string& key,
                                      const std::string& value) = 0;
};

class Manager
{
    public:
        Manager(sdbusplus::bus::bus& bus);
        void addListener(PropertyChangeListner* listener);
    private:
        sdbusplus::bus::bus& bus;
        sdbusplus::server::match::match propertyChangeMatch;
        std::set<PropertyChangeListner*> listeners;

        /** @brief Notified on host settings property changed */
        void onPropertyChanged(const std::string& key,
                               const std::string& value);
        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);
        static const std::set<std::string> managedProperties;
};

}
}
