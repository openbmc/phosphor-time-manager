#pragma once

#include "types.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

#include <fstream>

namespace phosphor
{
namespace time
{
namespace utils
{

using namespace phosphor::logging;

/** @brief Read data with type T from file */
template <typename T>
T readData(const char* fileName)
{
    T data{};
    std::ifstream fs(fileName);
    if (fs.is_open())
    {
        fs >> data;
    }
    return data;
}

/** @brief Write data with type T to file */
template <typename T>
void writeData(const char* fileName, T&& data)
{
    std::ofstream fs(fileName, std::ios::out);
    if (fs.is_open())
    {
        fs << std::forward<T>(data);
    }
}

/** @brief The template function to get property from DBus
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
T getProperty(sdbusplus::bus::bus& bus,
              const char* service,
              const char* path,
              const char* interface,
              const char* propertyName)
{
    sdbusplus::message::variant<T> value{};
    auto method = bus.new_method_call(service,
                                      path,
                                      "org.freedesktop.DBus.Properties",
                                      "Get");
    method.append(interface, propertyName);
    auto reply = bus.call(method);
    if (reply)
    {
        reply.read(value);
    }
    else
    {
        log<level::ERR>("Failed to get property",
                        entry("SERVICE=%s", service),
                        entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface),
                        entry("PROPERTY=%s", propertyName));
    }
    return value.template get<T>();
}

/** @brief Get service name from object path and interface
 *
 * @param[in] bus          - The Dbus bus object
 * @param[in] path         - The Dbus object path
 * @param[in] interface    - The Dbus interface
 *
 * @return The name of the service
 */
std::string getService(sdbusplus::bus::bus& bus,
                       const char* path,
                       const char* interface);

/** @brief Convert a string to enum Mode
 *
 * Convert the time mode string to enum.
 * Valid strings are "NTP", "MANUAL"
 * If it's not a valid time mode string, return MANUAL.
 *
 * @param[in] mode - The string of time mode
 *
 * @return The Mode enum
 */
Mode strToMode(const std::string& mode);

/** @brief Convert a string to enum Owner
 *
 * Convert the time owner string to enum.
 * Valid strings are "BMC", "HOST", "SPLIT", "BOTH"
 * If it's not a valid time owner string, return BOTH.
 *
 * @param[in] owner - The string of time owner
 *
 * @return The Owner enum
 */
Owner strToOwner(const std::string& owner);

/** @brief Convert a mode enum to mode string */
const char* modeToStr(Mode mode);

/** @brief Convert a owner enum to owner string */
const char* ownerToStr(Owner owner);

} // namespace utils
} // namespace time
} // namespace phosphor
