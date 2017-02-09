#pragma once

#include "bmc_time_change_listener.hpp"
#include "epoch_base.hpp"

#include <chrono>

namespace phosphor
{
namespace time
{

using namespace std::chrono;

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
        ~BmcEpoch();

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

        /** @brief Set the listner for bmc time change
         *
         * @param[in] listener - The pointer to the listener
         */
        void setBcmTimeChangeListener(BmcTimeChangeListener* listener);

    private:
        /** @brief The fd for time change event */
        int timeFd = -1;

        /** @brief Notify the listeners that bmc time is changed
         *
         * @param[in] time - The epoch time in microseconds to notify
         */
        void notifyBmcTimeChange(const microseconds& time);

        /** @brief The callback function on system time change
         *
         * @param[in] es - Source of the event
         * @param[in] fd - File descriptor of the timer
         * @param[in] revents - Not used
         * @param[in] userdata - User data pointer
         */
        static int onTimeChange(sd_event_source* es, int fd,
                                uint32_t revents, void* userdata);

        /** @brief The reference of sdbusplus bus */
        sdbusplus::bus::bus& bus;

        /** @brief The event source on system time change */
        sd_event_source* timeChangeEventSource = nullptr;

        /** @brief The listener for bmc time change */
        BmcTimeChangeListener* timeChangeListener = nullptr;
};

} // namespace time
} // namespace phosphor
