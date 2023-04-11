#pragma once

#include <xyz/openbmc_project/Time/Synchronization/server.hpp>

static constexpr auto objpathBmc = "/xyz/openbmc_project/time/bmc";
static constexpr auto busname = "xyz.openbmc_project.Time.Manager";

namespace phosphor
{
namespace time
{
/** @brief Alias to time sync mode class */
using ModeSetting =
    sdbusplus::xyz::openbmc_project::Time::server::Synchronization;

/** @brief Supported time sync modes
 *  NTP     Time sourced by Network Time Server
 *  Manual  User of the system need to set the time
 */
using Mode = ModeSetting::Method;
} // namespace time
} // namespace phosphor
