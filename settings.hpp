#pragma once

#include "config.h"

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <string>

namespace settings
{

constexpr auto root = "/";
constexpr auto timeSyncIntf = "xyz.openbmc_project.Time.Synchronization";
constexpr auto ntpSync = "xyz.openbmc_project.Time.Synchronization.Method.NTP";
constexpr auto manualSync =
    "xyz.openbmc_project.Time.Synchronization.Method.Manual";

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
    explicit Objects(sdbusplus::bus_t& /*bus*/);
    Objects() = delete;
    Objects(const Objects&) = delete;
    Objects& operator=(const Objects&) = delete;
    Objects(Objects&&) = default;
    Objects& operator=(Objects&&) = delete;
    ~Objects() = default;

    /** @brief time sync method settings object */
    phosphor::time::utils::Path timeSyncMethod = DEFAULT_TIME_SYNC_OBJECT_PATH;
};

} // namespace settings
