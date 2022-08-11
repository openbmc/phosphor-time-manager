#pragma once

#include "types.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <string_view>
#include <vector>

namespace phosphor
{
namespace time
{
namespace utils
{

using Path = std::string;
using Service = std::string;
using Interface = std::string;
using Interfaces = std::vector<Interface>;
using MapperResponse =
    std::vector<std::pair<Path, std::vector<std::pair<Service, Interfaces>>>>;

PHOSPHOR_LOG2_USING;

/** @brief The template function to get property from the requested dbus path
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 * @param[in] propertyName - The property name to get
 *
 * @return The value of the property
 */
template <typename T>
T getProperty(sdbusplus::bus_t& bus, const char* service, const char* path,
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
    catch (const sdbusplus::exception_t& ex)
    {
        error("GetProperty call failed, path:{PATH}, interface:{INTF}, "
              "propertyName:{NAME}, error:{ERROR}",
              "PATH", path, "INTF", interface, "NAME", propertyName, "ERROR",
              ex);
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
std::string getService(sdbusplus::bus_t& bus, const char* path,
                       const char* interface);

/** @brief Get sub tree from root, depth and interfaces
 *
 * @param[in] bus           - The Dbus bus object
 * @param[in] root          - The root of the tree to search
 * @param[in] interfaces    - All interfaces in the subtree to search for
 * @param[in] depth         - The number of path elements to descend
 *
 * @return The name of the service
 *
 * @throw sdbusplus::exception_t when it fails
 */
MapperResponse getSubTree(sdbusplus::bus_t& bus, const std::string& root,
                          const Interfaces& interfaces, int32_t depth);

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

} // namespace utils
} // namespace time
} // namespace phosphor
