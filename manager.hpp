#pragma once

#include "types.hpp"
#include "property_change_listener.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>

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

        /** @brief Add a listener that will be called
          * when property is changed
         **/
        void addListener(PropertyChangeListner* listener);

    private:
        /** @brief Persistent sdbusplus DBus connection */
        sdbusplus::bus::bus& bus;

        /** @brief The match of host setting property change */
        sdbusplus::bus::match::match propertyChangeMatch;

        /** @brief The match of pgood change */
        sdbusplus::bus::match::match pgoodChangeMatch;

        /** @brief The container to hold all the listeners */
        std::set<PropertyChangeListner*> listeners;

        /** @brief The value to indicate if host is on */
        bool hostOn;

        /** @brief The current time mode */
        Mode timeMode;

        /** @brief The current time owner */
        Owner timeOwner;

        /** @brief Check if host is on and update hostOn variable */
        void checkHostOn();

        /** @brief Get setting from settingsd service
         *
         * @param[in] setting - The string of the setting
         *
         * @return The setting value in string
         */
        std::string getSettings(const char* setting) const;

        /** @brief Set current time mode
         *
         * @param[in] mode - The string of time mode
         */
        void setCurrentTimeMode(const std::string& mode);

        /** @brief Set current time owner
         *
         * @param[in] owner - The string of time owner
         */
        void setCurrentTimeOwner(const std::string& owner);

        /** @brief Notified on host settings property changed
         *
         * @param[in] key - The name of property that is changed
         * @param[in] value - The value of the property
         */
        void onPropertyChanged(const std::string& key,
                               const std::string& value);

        /** @brief Notified on pgood has changed
         *
         * @param[in] pgood - The changed pgood value
         */
        void onPgoodChanged(bool pgood);

        /** @brief Notified on host settings property changed
         *
         * @param[in] msg - Data associated with subscribed signal
         * @param[in] userData - Pointer to this object instance
         * @param[out] retError  - Not used but required with signal API
         */
        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);

        /** @brief Notified on pgood has changed
         *
         * @param[in] msg - Data associated with subscribed signal
         * @param[in] userData - Pointer to this object instance
         * @param[out] retError  - Not used but required with signal API
         */
        static int onPgoodChanged(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retError);

        /** @brief Convert a string to enum Mode
         *
         * Convert the time mode string to enum.
         * Valid strings are "NTP", "MANUAL"
         * If it's not a valid time mode string, return NTP.
         *
         * @param[in] mode - The string of time mode
         *
         * @return The Mode enum
         */
        static Mode convertToMode(const std::string& mode);

        /** @brief Convert a string to enum Owner
         *
         * Convert the time owner string to enum.
         * Valid strings are "BMC", "HOST", "SPLIT", "BOTH"
         * If it's not a valid time owner string, return BMC.
         *
         * @param[in] owner - The string of time owner
         *
         * @return The Owner enum
         */
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
