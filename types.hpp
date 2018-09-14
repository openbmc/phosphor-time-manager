#pragma once

#include <xyz/openbmc_project/Time/Owner/server.hpp>
#include <xyz/openbmc_project/Time/Synchronization/server.hpp>

namespace phosphor
{
namespace time
{
/** @brief Alias to time sync mode class */
using ModeSetting =
    sdbusplus::xyz::openbmc_project::Time::server::Synchronization;

/** @brief Alias to time owner class */
using OwnerSetting = sdbusplus::xyz::openbmc_project::Time::server::Owner;

/** @brief Supported time sync modes
 *  NTP     Time sourced by Network Time Server
 *  Manual  User of the system need to set the time
 */
using Mode = ModeSetting::Method;

/** @brief Supported time owners
 *  BMC     Time source may be NTP or Manual but it has to be set natively
 *          on the BMC. Meaning, host can not set the time. What it also
 *          means is that when BMC gets IPMI_SET_SEL_TIME, then its ignored.
 *          similarly, when BMC gets IPMI_GET_SEL_TIME, then the BMC's time
 *          is returned.
 *
 *  Host    Its only IPMI_SEL_SEL_TIME that will set the time on BMC.
 *          Meaning, IPMI_GET_SEL_TIME and request to get BMC time will
 *          result in same value.
 *
 *  Split   Both BMC and Host will maintain their individual clocks but then
 *          the time information is stored in BMC. BMC can have either NTP
 *          or Manual as it's source of time and will set the time directly
 *          on the BMC. When IPMI_SET_SEL_TIME is received, then the delta
 *          between that and BMC's time is calculated and is stored.
 *          When BMC reads the time, the current time is returned.
 *          When IPMI_GET_SEL_TIME is received, BMC's time is retrieved and
 *          then the delta offset is factored in prior to returning.
 *
 *  Both:   BMC's time is set with whoever that sets the time. Similarly,
 *          BMC's time is returned to whoever that asks the time.
 */
using Owner = OwnerSetting::Owners;
} // namespace time
} // namespace phosphor
