#pragma once

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
                  const char* objPath,
                  Manager* manager);

        uint64_t elapsed() const override;
        uint64_t elapsed(uint64_t value) override;

    private:
        std::chrono::microseconds offset;

        // Store the offset in File System. Read back when starts.
        static constexpr auto offsetFile = "/var/lib/obmc/saved_host_offset";

        template <typename T>
        static T readData(const char* fileName);
        template <typename T>
        static void writeData(const char* fileName, T&& data);
};

}
}
