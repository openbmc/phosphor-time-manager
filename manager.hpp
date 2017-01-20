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

class Manager
{
    public:
        friend class TestManager;
        Manager(sdbusplus::bus::bus& bus);
        void addListener(PropertyChangeListner* listener);

    private:
        sdbusplus::bus::bus& bus;

        /** @brief The match of settings' property changed */
        sdbusplus::server::match::match propertyChangeMatch;

        /** @brief The listeners of to notify on property changed */
        std::set<PropertyChangeListner*> listeners;

        /** @brief The current time mode */
        Mode timeMode;

        /** @brief The current time owner */
        Owner timeOwner;

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

        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);

        /** @brief Convert a string to enum Mode
         *
         * Convert the time mode string to enum.
         * Valid strings are "NTP", "MANUAL"
         * If it's not a valid time mode string, return NTP.
         *
         * @param[in] mode - The string of time mode
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
         * @return The Owner enum
         */
        static Owner convertToOwner(const std::string& owner);

        using Updater = std::function<void(const std::string&)>;

        /** @brief The helper map to call property update function
            on property changed
         */
        const std::map<std::string, Updater> propertyUpdaters =
        {
            {"time_mode", std::bind(&Manager::setCurrentTimeMode,
                                    this, std::placeholders::_1)},
            {"time_owner", std::bind(&Manager::setCurrentTimeOwner,
                                     this, std::placeholders::_1)}
        };

        static const std::set<std::string> managedProperties;
        static const std::map<std::string, Owner> ownerMap;
};

}
}
