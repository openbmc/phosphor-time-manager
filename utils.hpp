#pragma once

#include "config.h"

#include "types.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

#include <chrono>
#include <iostream>

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
    catch (const sdbusplus::exception::exception& ex)
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

typedef struct
{
    uint8_t netFn;
    uint8_t lun;
    uint8_t cmd;
    std::vector<uint8_t> ipmbAddr;
    std::vector<uint8_t> cmdData;
    std::string bridgeInterface;
} ipmbCmdInfo;

/** @brief callback to get the host time and set that time to bmc
 *
 * @param[in] bus data - the dbus object
 *
 * @param[in] ipmbCmdInfo the host time ipmb command
 *
 * @param[out] respData - vector of response data
 *
 * @return bool

 */
bool readHostTimeViaIpmb(sdbusplus::bus::bus& bus, ipmbCmdInfo hostTimeCmd,
                         std::vector<uint8_t>& respData);

/** @brief Read IPMB SEL information (netfn,lun,hostid,cmd,cmdData) from
 * configuration file
 * @param[in] bus data - the dbus object
 *
 * @return return the ipmg host time read cmd info
 *
 */
ipmbCmdInfo loadIPMBCmd(sdbusplus::bus::bus& bus);

/** @brief parse the response data to epoch time
 *
 * @param[in] reponse data - vector of response data
 *
 * @return return the epoch time
 */
uint64_t parseToEpoch(std::vector<uint8_t>& respData);

/** @brief parse the response data to epoch time
 *
 * @param[in] bus data - the dbus object
 *
 * @return return the epoch time
 */
uint64_t getTimeFromHost(sdbusplus::bus::bus& bus);

/** @brief parse the response data to epoch time
 *
 * @param[in] bus data - the dbus object
 *
 * @return return the epoch time
 */
uint64_t getHostTimeViaIpmb(sdbusplus::bus::bus& bus);

/** @brief get the time from host and store in bmc time
 *
 * @param[in] bus           - The Dbus bus object
 *
 * @return return nothing
 */
void updateBmcTimeFromHost(sdbusplus::bus::bus& bus);

const std::vector<std::string> getSubTreePaths(sdbusplus::bus::bus& bus,
                                               const std::string& objectPath,
                                               const std::string& interface);
const std::string getProperty(sdbusplus::bus::bus& bus,
                              const std::string& objectPath,
                              const std::string& interface,
                              const std::string& propertyName);
std::vector<uint8_t> strToIntArray(std::string stream);

} // namespace utils
} // namespace time
} // namespace phosphor
