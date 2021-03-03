#pragma once

#include "config.h"

#include "property_change_listener.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{

/** @class EpochBase
 *  @brief Base class for OpenBMC EpochTime implementation.
 *  @details A base class that implements xyz.openbmc_project.Time.EpochTime
 *  DBus API for epoch time.
 */
class EpochBase :
    public sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Time::server::EpochTime>,
    public PropertyChangeListner
{
  public:
    friend class TestEpochBase;

    EpochBase(sdbusplus::bus::bus& bus, const char* objPath);

    /** @brief Notified on time mode changed */
    void onModeChanged(Mode mode) override;

  protected:
    /** @brief Persistent sdbusplus DBus connection */
    sdbusplus::bus::bus& bus;

    /** @brief The current time mode */
    Mode timeMode = DEFAULT_TIME_MODE;
 
    /** @brief Get current time
     *
     * @return Microseconds since UTC
     */
    std::chrono::microseconds getTime() const;
};

} // namespace time
} // namespace phosphor
