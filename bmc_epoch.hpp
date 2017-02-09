#pragma once

#include "bmc_time_change_listener.hpp"
#include "epoch_base.hpp"

namespace phosphor
{
namespace time
{

/** @class BmcEpoch
 *  @brief OpenBMC BMC EpochTime implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Time.EpochTime
 *  DBus API for BMC's epoch time.
 */
class BmcEpoch : public EpochBase
{
    public:
        friend class TestBmcEpoch;
        BmcEpoch(sdbusplus::bus::bus& bus,
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

        /** @brief Set the listner for bmc time change */
        void setBcmTimeChangeListener(BmcTimeChangeListener* listener);

    private:
        /** @brief The listener for bmc time change */
        BmcTimeChangeListener* timeChangeListener = nullptr;
};

} // namespace time
} // namespace phosphor
