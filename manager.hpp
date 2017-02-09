#pragma once

#include "types.hpp"
#include "property_change_listener.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/match.hpp>

#include <set>
#include <string>

namespace phosphor
{
namespace time
{

/** @class Manager
 *  @brief The manager to handle OpenBMC time.
 *  @details It registers various time related settings and properties signals
 *  on DBus and handle the changes.
 *  For certain properties it also notifies the changed events to listeners.
 */
class Manager
{
    public:
        friend class TestManager;
        Manager(sdbusplus::bus::bus& bus);
        void addListener(PropertyChangeListner* listener);

    private:
        sdbusplus::bus::bus& bus;
        sdbusplus::server::match::match propertyChangeMatch;
        std::set<PropertyChangeListner*> listeners;

        Mode timeMode;
        Owner timeOwner;

        std::string getSettings(const char* setting) const;

        void setCurrentTimeMode(const std::string& mode);
        void setCurrentTimeOwner(const std::string& owner);

        /** @brief Notified on host settings property changed */
        void onPropertyChanged(const std::string& key,
                               const std::string& value);

        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);

        static Mode convertToMode(const std::string& mode);
        static Owner convertToOwner(const std::string& owner);

        using Updater = std::function<void(const std::string&)>;

        /** @brief Map the property string to functions that shall
         *  be called when the property is changed
         */
        const std::map<std::string, Updater> propertyUpdaters =
        {
            {"time_mode", std::bind(&Manager::setCurrentTimeMode,
                                    this, std::placeholders::_1)},
            {"time_owner", std::bind(&Manager::setCurrentTimeOwner,
                                     this, std::placeholders::_1)}
        };

        /** @brief The properties that manager shall notify the
         *  listeners when changed
         */
        static const std::set<std::string> managedProperties;

        /** @brief The map that maps the string to Owners */
        static const std::map<std::string, Owner> ownerMap;
};

}
}
