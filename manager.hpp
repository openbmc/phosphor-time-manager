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

        /** @brief The match of settings property change */
        sdbusplus::bus::match::match propertyChangeMatch;

        /** @brief The match of pgood change */
        sdbusplus::bus::match::match pgoodChangeMatch;

        /** @brief The container to hold all the listeners */
        std::set<PropertyChangeListner*> listeners;

        /** @brief The value to indicate if host is on */
        bool hostOn = false;

        /** @brief The requested time mode when host is on*/
        std::string requestedMode;

        /** @brief The requested time owner when host is on*/
        std::string requestedOwner;

        /** @brief The current time mode */
        Mode timeMode;

        /** @brief The current time owner */
        Owner timeOwner;

        /** @brief Restore saved settings */
        void restoreSettings();

        /** @brief Check if host is on and update hostOn variable */
        void checkHostOn();

        /** @brief Check if use_dhcp_ntp is used and update NTP setting */
        void checkDhcpNtp();

        /** @brief Get setting from settingsd service
         *
         * @param[in] setting - The string of the setting
         *
         * @return The setting value in string
         */
        std::string getSettings(const char* setting) const;

        /** @brief Set current time mode from the time mode string
         *
         * @param[in] mode - The string of time mode
         *
         * @return - true if the mode is updated
         *           false if it's the same as before
         */
        bool setCurrentTimeMode(const std::string& mode);

        /** @brief Set current time owner from the time owner string
         *
         * @param[in] owner - The string of time owner
         *
         * @return - true if the owner is updated
         *           false if it's the same as before
         */
        bool setCurrentTimeOwner(const std::string& owner);

        /** @brief Called on time mode is changed
         *
         * Notify listeners that time mode is changed and update ntp setting
         *
         * @param[in] mode - The string of time mode
         */
        void onTimeModeChanged(const std::string& mode);

        /** @brief Called on time owner is changed
         *
         * Notify listeners that time owner is changed
         */
        void onTimeOwnerChanged();

        /** @brief Notified on settings property changed
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

        /** @brief Set the property as requested time mode/owner
         *
         * @param[in] key - The property name
         * @param[in] value - The property value
         */
        void setPropertyAsRequested(const std::string& key,
                                    const std::string& value);

        /** @brief Set the current mode to user requested one
         *  if conditions allow it
         *
         * @param[in] mode - The string of time mode
         */
        void setRequestedMode(const std::string& mode);

        /** @brief Set the current owner to user requested one
         *  if conditions allow it
         *
         * @param[in] owner - The string of time owner
         */
        void setRequestedOwner(const std::string& owner);

        /** @brief Update the NTP setting to systemd time service
         *
         * @param[in] value - The time mode value, e.g. "NTP" or "MANUAL"
         */
        void updateNtpSetting(const std::string& value);

        /** @brief Update dhcp_ntp setting to OpenBMC network service
         *
         * @param[in] value - The use_dhcp_ntp value, e.g. "yes" or "no"
         */
        void updateDhcpNtpSetting(const std::string& useDhcpNtp);

        /** @brief The static function called on settings property changed
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

        /** @brief The string of time mode property */
        static constexpr auto PROPERTY_TIME_MODE = "time_mode";

        /** @brief The string of time owner property */
        static constexpr auto PROPERTY_TIME_OWNER = "time_owner";

        /** @brief The string of use dhcp ntp property */
        static constexpr auto PROPERTY_DHCP_NTP = "use_dhcp_ntp";

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

        /** @brief The file name of saved time mode */
        static constexpr auto modeFile = "/var/lib/obmc/saved_time_mode";

        /** @brief The file name of saved time owner */
        static constexpr auto ownerFile = "/var/lib/obmc/saved_time_owner";
};

}
}
