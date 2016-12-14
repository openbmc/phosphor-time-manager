#pragma once

#include <systemd/sd-bus.h>
#include <fstream>
#include <string>
#include <chrono>
#include "xyz/openbmc_project/Time/EpochTime/server.hpp"
#include <time.h>
#include <sdbusplus/server.hpp>

namespace phosphor
{
namespace time
{

namespace detail
{
template <typename T>
using ServerObject = typename sdbusplus::server::object::object<T>;

using EpochTimeIface =
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime;

} // namespace detail


/** @class EpochTime
 *  @brief OpenBMC manager of EpochTime implementation.
 *  @details A concrete implementation for the
 *  xyz.openbmc_project.Time.EpochTime DBus API.
 */
class EpochTime final :
    public detail::ServerObject<detail::EpochTimeIface>
{
    public:
        EpochTime() = delete;
        EpochTime(const EpochTime&) = delete;
        EpochTime& operator=(const EpochTime&) = delete;
        EpochTime(EpochTime&&) = default;
        EpochTime& operator=(EpochTime&&) = default;
        ~EpochTime() = default;

        /** @brief Constructor for the Log Manager object
         *  @param[in] bus - DBus bus to attach to.
         *  @param[in] busname - Name of DBus bus to own.
         *  @param[in] obj - Object path to attach to.
         */
        EpochTime(sdbusplus::bus::bus&& bus,
                    const char* busname,
                    const char* obj);

        /** @brief Start processing DBus messages. */
        void run() noexcept;

        /** @brief common routine of reading bmc time */
        std::chrono::microseconds getBaseTime() const;
        /*
         * @fn elapsed()
         * @brief sd_bus elapsed method implementation callback.
         * @details 
         */
        uint64_t elapsed() const override;

    private:
        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus _bus;

        /** @brief sdbusplus org.freedesktop.DBus.ObjectManager reference. */
        sdbusplus::server::manager::manager _manager;
 
        /** @brief object path of this time manager */
        std::string objpath;
};

} // namespace time
} // namespace phosphor
