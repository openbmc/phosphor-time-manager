#pragma once

#include "types.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{
namespace utils
{

using namespace phosphor::logging;

/** @brief The template function to get property from the requested dbus path
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] service      - The Dbus service name
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 * @param[in] propertyName - The property name to get
 *
 * @return The value of the property
 */
template <typename T>
T getProperty(sdbusplus::bus::bus& bus, const char* service, const char* path,
              const char* interface, const char* propertyName)
{
    auto method = bus.new_method_call(service, path,
                                      "org.freedesktop.DBus.Properties", "Get");
    method.append(interface, propertyName);
    try
    {
        std::variant<T> value{};
        auto reply = bus.call(method);
        reply.read(value);
        return std::get<T>(value);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("GetProperty call failed", entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface),
                        entry("PROPERTY=%s", propertyName));
        throw std::runtime_error("GetProperty call failed");
    }
}

/** @brief Get service name from object path and interface
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 *
 * @return The name of the service
 */
std::string getService(sdbusplus::bus::bus& bus, const char* path,
                       const char* interface);

/** @brief Convert a string to enum Mode
 *
 * Convert the time mode string to enum.
 * Valid strings are
 *   "xyz.openbmc_project.Time.Synchronization.Method.NTP"
 *   "xyz.openbmc_project.Time.Synchronization.Method.Manual"
 * If it's not a valid time mode string, it means something
 * goes wrong so raise exception.
 *
 * @param[in] mode - The string of time mode
 *
 * @return The Mode enum
 */
Mode strToMode(const std::string& mode);

/** @brief Convert a mode enum to mode string
 *
 * @param[in] mode - The Mode enum
 *
 * @return The string of the mode
 */
std::string modeToStr(Mode mode);

/** @brief The function to set time of BMC
 *
 * @param[in] bus           - The Dbus bus object
 * @param[in] timeofDayUsec - Time in microseconds since EPOCH
 *
 * @return The value of the property
 */
bool setTime(sdbusplus::bus::bus& bus,
             const std::chrono::microseconds& timeOfDayUsec);

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
bool readHostTimeViaIpmb(sdbusplus::bus::bus& bus, std::vector<uint8_t>& hostId,
                         uint8_t netFn, uint8_t lun, uint8_t cmd,
                         std::vector<uint8_t>& cmdData,
                         std::vector<uint8_t>& respData,
                         std::string bridgeInterface);

/** @brief Read IPMB cell information (netfn,lun,hostid,cmd,cmdData) from
 * configuration file
 *
 *  @param[in] netFn - to store netFn value from configuration file
 *  @param[in] lun   - to store lun value from configuration file
 *  @param[in] cmd   - to store cmd value from configuration file
 *  @param[in] cmdData   - to store cmd value from configuration file
 *  @param[in] cmd   - to store cmd value from configuration file
 *  @param[in] bridgeInterface - to store bridge interface from
 * configuration file
 *
 */
void loadIPMBCmd(sdbusplus::bus::bus& bus, uint8_t& netFn, uint8_t& lun,
                 uint8_t& cmd, std::vector<uint8_t>& hostData,
                 std::vector<uint8_t>& cmdData, std::string& bridgeInterface);

/** @brief parse the response data to epoch time
 *
 * @param[in] reponse data - vector of response data
 *
 * @return return the epoch time
 */
uint64_t parseToEpoch(std::vector<uint8_t>& respData);
/** @brief get the time from host and store in bmc time
 *
 * @param[in] bus           - The Dbus bus object
 *
 * @return return nothing
 */

void updateBmcTimeFromHost(sdbusplus::bus::bus& bus);

} // namespace utils
} // namespace time
} // namespace phosphor
