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

        /** @brief The match of pgood changed */
        sdbusplus::server::match::match pgoodChangeMatch;

        /** @brief The listeners of to notify on property changed */
        std::set<PropertyChangeListner*> listeners;

        /** @brief The value to indicate if host is on */
        bool isHostOn;

        /** @brief The requested time mode when host is on*/
        std::string requestedMode;

        /** @brief The requested time owner when host is on*/
        std::string requestedOwner;

        /** @brief The current time mode */
        Mode timeMode;

        /** @brief The current time owner */
        Owner timeOwner;

        void initPgood();

        /** @brief Get setting value from org.openbmc.settings.Host.
         *
         * @param[in] setting - The string of the setting to get
         * @return The value of the setting
         */
        std::string getSettings(const char* setting) const;

        void setCurrentTimeMode(const std::string& mode);
        void setCurrentTimeOwner(const std::string& owner);

        /** @brief Notified on host settings property changed */
        void onPropertyChanged(const std::string& key,
                               const std::string& value);
        /** @brief Notified on pgood has changed */
        void onPgoodChanged(bool pgood);
        void saveProperty(const std::string& key,
                          const std::string& value);
        void setRequestedMode(const std::string& mode);
        void setRequestedOwner(const std::string& owner);

        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);
        static int onPgoodChanged(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retError);

        static Mode convertToMode(const std::string& mode);
        static Owner convertToOwner(const std::string& owner);

        static constexpr auto PROPERTY_TIME_MODE = "time_mode";
        static constexpr auto PROPERTY_TIME_OWNER = "time_owner";

        using Updater = std::function<void(const std::string&)>;

        /** @brief Map the property string to functions that shall
         *  be called when the property is changed
         */
        const std::map<std::string, Updater> propertyUpdaters =
        {
            {PROPERTY_TIME_MODE, std::bind(&Manager::setCurrentTimeMode,
                                           this, std::placeholders::_1)},
            {PROPERTY_TIME_OWNER, std::bind(&Manager::setCurrentTimeOwner,
                                            this, std::placeholders::_1)}
        };

        /** @brief The properties that manager shall notify the
         *  listeners when changed
         */
        static const std::set<std::string> managedProperties;

        /** @brief The map that maps the string to Owners */
        static const std::map<std::string, Owner> ownerMap;

        static constexpr auto modeFile = "/var/lib/obmc/saved_time_mode";
        static constexpr auto ownerFile = "/var/lib/obmc/saved_time_owner";
};

}
}
