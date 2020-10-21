#pragma once

#include "config.h"

#include "property_change_listener.hpp"
#include "settings.hpp"
#include "types.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/container/flat_map.hpp>

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

    explicit Manager(sdbusplus::bus::bus& bus);
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    ~Manager() = default;

  private:
    /** @brief Persistent sdbusplus DBus connection */
    sdbusplus::bus::bus& bus;

    /** @brief The match of settings property change */
    std::vector<sdbusplus::bus::match::match> settingsMatches;

    /** @brief Settings objects of intereset */
    settings::Objects settings;

    /** @brief The current time mode */
    Mode timeMode = DEFAULT_TIME_MODE;
	
    /** @brief resonse data from ipmb method call */
    std::vector<uint8_t> respData;
	
    /** @brief callback to get the host time and set that time to bmc
     *
     * @param[in] hostId - host port ID
     * @param[in] netFn - net function value
     * @param[in] lun - logical time unit value
     * @param[in] cmd - request message to bridge
     * @param[in] cmdData - vector of cmdData
     * @param[in] respData - vector of response data
     * @param[in] bridgeInterface - bridge between host and BMC
     *
     */
    void updateHostSyncSetting(std::vector<uint8_t>&  hostId, uint8_t netFn, uint8_t lun, uint8_t cmd,
                              std::vector<uint8_t>& cmdData, std::vector<uint8_t>& respData,
                              std::string bridgeInterface);
	
    /** @brief parse the response data to epoch time
     *
     * @param[in] reponse data - vector of response data
     * 
     * @return return the epoch time
     */
     uint64_t parseToEpoch(std::vector<uint8_t>& respData);
	
    /** @brief Read IPMB cell information (netfn,lun,hostid,cmd,cmdData) from configuration file
     *
     *  @param[in] netFn - to store netFn value from configuration file
     *  @param[in] lun   - to store lun value from configuration file
     *  @param[in] cmd   - to store cmd value from configuration file
     *  @param[in] cmdData   - to store cmd value from configuration file
     *  @param[in] cmd   - to store cmd value from configuration file
     *  @param[in] bridgeInterface - to store bridge interface from configuration file 
     *
     */
    void readFromJson(uint8_t &netFn, uint8_t &lun, uint8_t &cmd, std::vector<uint8_t> &hostData,
                     std::vector<uint8_t> &cmdData, std::string &bridgeInterface);

    /** @brief Get setting from settingsd service
     *
     * @param[in] path - The dbus object path
     * @param[in] interface - The dbus interface
     * @param[in] setting - The string of the setting
     *
     * @return The setting value in string
     */
    std::string getSetting(const char* path, const char* interface,
                           const char* setting) const;

    /** @brief Set current time mode from the time mode string
     *
     * @param[in] mode - The string of time mode
     *
     * @return - true if the mode is updated
     *           false if it's the same as before
     */
    bool setCurrentTimeMode(const std::string& mode);
    /** @brief Called on time mode is changed
     *
     * Notify listeners that time mode is changed and update ntp setting
     *
     * @param[in] mode - The string of time mode
     */
    void onTimeModeChanged(const std::string& mode);

    /** @brief Callback to handle change in a setting
     *
     *  @param[in] msg - sdbusplus dbusmessage
     *
     *  @return 0 on success, < 0 on failure.
     */
    int onSettingsChanged(sdbusplus::message::message& msg);

    /** @brief Notified on settings property changed
     *
     * @param[in] key - The name of property that is changed
     * @param[in] value - The value of the property
     */
    void onPropertyChanged(const std::string& key, const std::string& value);

    /** @brief Update the NTP setting to systemd time service
     *
     * @param[in] value - The time mode value, e.g. "NTP" or "MANUAL"
     */
    void updateNtpSetting(const std::string& value);

    /** @brief The static function called on settings property changed
     *
     * @param[in] msg - Data associated with subscribed signal
     * @param[in] userData - Pointer to this object instance
     * @param[out] retError  - Not used but required with signal API
     */
    static int onPropertyChanged(sd_bus_message* msg, void* userData,
                                 sd_bus_error* retError);

    /** @brief The string of time mode property */
    static constexpr auto PROPERTY_TIME_MODE = "TimeSyncMethod";
};

} // namespace time
} // namespace phosphor
