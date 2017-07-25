#pragma once

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
class EpochBase : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime >,
    public PropertyChangeListner
{
    public:
        friend class TestEpochBase;

        EpochBase(sdbusplus::bus::bus& bus,
                  const char* objPath);

        /** @brief Notified on time mode changed */
        void onModeChanged(Mode mode) override;

        /** @brief Notified on time owner changed */
        void onOwnerChanged(Owner owner) override;

    protected:
        /** @brief Persistent sdbusplus DBus connection */
        sdbusplus::bus::bus& bus;

        /** @brief The current time mode */
        Mode timeMode = Mode::Manual;

        /** @brief The current time owner */
        Owner timeOwner = Owner::Both;

        /** @brief Set current time to system
         *
         * This function set the time to system by invoking systemd
         * org.freedesktop.timedate1's SetTime method.
         *
         * @param[in] timeOfDayUsec - Microseconds since UTC
         */
        void setTime(const std::chrono::microseconds& timeOfDayUsec);

        /** @brief Get current time
         *
         * @return Microseconds since UTC
         */
        std::chrono::microseconds getTime() const;
};

} // namespace time
} // namespace phosphor
