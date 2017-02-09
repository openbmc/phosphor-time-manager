#pragma once

#include "bmc_time_change_listener.hpp"
#include "config.h"
#include "epoch_base.hpp"

#include <chrono>

namespace phosphor
{
namespace time
{

/** @class HostEpoch
 *  @brief OpenBMC HOST EpochTime implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Time.EpochTime
 *  DBus API for HOST's epoch time.
 */
class HostEpoch : public EpochBase, public BmcTimeChangeListener
{
    public:
        friend class TestHostEpoch;
        HostEpoch(sdbusplus::bus::bus& bus,
                  const char* objPath);

        /**
         * @brief Get value of Elapsed property
         *
         * @return The elapsed microseconds since UTC
         **/
        uint64_t elapsed() const override;

        /**
         * @brief Set value of Elapsed property
         *
         * @param[in] value - The microseconds since UTC to set
         * @return The updated elapsed microseconds since UTC
         **/
        uint64_t elapsed(uint64_t value) override;

        /** @brief Notified on bmc time is changed */
        void onBmcTimeChanged(std::chrono::microseconds bmcTime) override;

    private:
        /** @brief The diff between BMC and Host time */
        std::chrono::microseconds offset;

        /**
         * @brief The diff between host time and steady clock
         * @details This diff is used to calculate the host time if BMC time
         * is changed and the owner is SPLIT.
         * Without this the host time is lost if BMC time is changed.
        */
        std::chrono::microseconds diffToSteadyClock;

        /** @brief The file to store the offset in File System.
         *  Read back when starts
         **/
        static constexpr auto offsetFile = HOST_OFFSET_FILE;
};

} // namespace time
} // namespace phosphor
