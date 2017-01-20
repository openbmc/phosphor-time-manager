#pragma once

#include "epoch_base.hpp"

namespace phosphor
{
namespace time
{

class BmcEpoch : public EpochBase
{
    public:
        friend class TestBmcEpoch;
        BmcEpoch(sdbusplus::bus::bus& bus,
                 const char* objPath);

        uint64_t elapsed() const override;
        uint64_t elapsed(uint64_t value) override;
};

}
}
