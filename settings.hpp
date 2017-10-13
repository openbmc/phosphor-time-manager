#pragma once

#include <string>
#include <sdbusplus/bus.hpp>

namespace settings
{

using Path = std::string;
using Service = std::string;
using Interface = std::string;

constexpr auto root = "/";
constexpr auto timeOwnerIntf = "xyz.openbmc_project.Time.Owner";
constexpr auto timeSyncIntf = "xyz.openbmc_project.Time.Synchronization";
constexpr auto hostStateIntf = "xyz.openbmc_project.State.Host";

/** @class Objects
 *  @brief Fetch paths of settings D-bus objects of interest upon construction
 */
struct Objects
{
    public:
        /** @brief Constructor - fetch settings objects
         *
         * @param[in] bus - The D-bus bus object
         */
        Objects();
        Objects(const Objects&) = default;
        Objects& operator=(const Objects&) = default;
        Objects(Objects&&) = default;
        Objects& operator=(Objects&&) = default;
        ~Objects() = default;

        /** @brief Fetch D-bus service, given a path and an interface. The
         *         service can't be cached because mapper returns unique
         *         service names.
         *
         * @param[in] path - The D-bus object
         * @param[in] interface - The D-bus interface
         *
         * @return std::string - the D-bus service
         */
        Service service(const Path& path, const Interface& interface) const;

        /** @brief time owner settings object */
        Path timeOwner;

        /** @brief time sync method settings object */
        Path timeSyncMethod;

        /** @brief host state object */
        Path hostState;
};

} // namespace settings
