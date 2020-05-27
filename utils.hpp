#pragma once

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

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

} // namespace utils
} // namespace time
} // namespace phosphor
