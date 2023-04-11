#pragma once

#include "manager.hpp"
#include "property_change_listener.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{

using namespace std::chrono;

using EpochTimeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime>;

/** @class BmcEpoch
 *  @brief OpenBMC BMC EpochTime implementation.
 *  @details A concrete implementation for
 * xyz.openbmc_project.Time.EpochTime DBus API for BMC's epoch time.
 */
class BmcEpoch : public EpochTimeIntf, public PropertyChangeListner
{
  public:
    BmcEpoch(sdbusplus::bus_t& bus, const char* objPath, Manager& manager) :
        EpochTimeIntf(bus, objPath), bus(bus), manager(manager)
    {
        initialize();
    }

    ~BmcEpoch();

    /** @brief Notified on time mode changed */
    void onModeChanged(Mode mode) override;

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

  protected:
    /** @brief Persistent sdbusplus DBus connection */
    sdbusplus::bus_t& bus;

    /** @brief The manager to handle OpenBMC time */
    Manager& manager;

    /** @brief Set current time to system
     *
     * This function set the time to system by invoking systemd
     * org.freedesktop.timedate1's SetTime method.
     *
     * @param[in] timeOfDayUsec - Microseconds since UTC
     *
     * @return true or false to indicate if it sets time successfully
     */
    bool setTime(const std::chrono::microseconds& timeOfDayUsec);

    /** @brief Get current time
     *
     * @return Microseconds since UTC
     */
    static std::chrono::microseconds getTime();

  private:
    /** @brief The fd for time change event */
    int timeFd = -1;

    /** @brief Initialize timerFd related resource */
    void initialize();

    /** @brief The callback function on system time change
     *
     * @param[in] es - Source of the event
     * @param[in] fd - File descriptor of the timer
     * @param[in] revents - Not used
     * @param[in] userdata - User data pointer
     */
    static int onTimeChange(sd_event_source* es, int fd, uint32_t revents,
                            void* userdata);

    /** @brief The deleter of sd_event_source */
    std::function<void(sd_event_source*)> sdEventSourceDeleter =
        [](sd_event_source* p) {
            if (p != nullptr)
            {
                sd_event_source_unref(p);
            }
        };
    using SdEventSource =
        std::unique_ptr<sd_event_source, decltype(sdEventSourceDeleter)>;

    /** @brief The event source on system time change */
    SdEventSource timeChangeEventSource{nullptr, sdEventSourceDeleter};
};

} // namespace time
} // namespace phosphor
