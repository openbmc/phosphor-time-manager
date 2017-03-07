#pragma once

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
class HostEpoch : public EpochBase
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

    private:
        std::chrono::microseconds offset;

        // Store the offset in File System. Read back when starts.
        static constexpr auto offsetFile = HOST_OFFSET_FILE;

        template <typename T>
        static T readData(const char* fileName);
        template <typename T>
        static void writeData(const char* fileName, T&& data);
};

} // namespace time
} // namespace phosphor
