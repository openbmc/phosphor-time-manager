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

        uint64_t elapsed() const override;
        uint64_t elapsed(uint64_t value) override;

    private:
        std::chrono::microseconds offset;

        // Store the offset in File System. Read back when starts.
        static constexpr auto offsetFile = SAVED_HOST_OFFSET_FILE;
};

}
}
