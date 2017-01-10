#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "bmc_epoch.hpp"
#include "config.h"

namespace phosphor
{
namespace time
{

class TestBmcEpoch : public testing::Test
{
    public:
        using Mode = EpochBase::Mode;
        using Owner = EpochBase::Owner;

        sdbusplus::bus::bus bus;
        BmcEpoch bmcEpoch;

        TestBmcEpoch()
            : bus(sdbusplus::bus::new_default()),
              bmcEpoch(bus, OBJPATH_BMC)
        {
            // Empty
        }

        // Proxies for BmcEpoch's private members and functions
        Mode getTimeMode()
        {
            return bmcEpoch.timeMode;
        }
        Owner getTimeOwner()
        {
            return bmcEpoch.timeOwner;
        }
};

TEST_F(TestBmcEpoch, empty)
{
    EXPECT_EQ(Mode::NTP, getTimeMode());
    EXPECT_EQ(Owner::BMC, getTimeOwner());
}

TEST_F(TestBmcEpoch, getElapsed)
{
    auto t1 = bmcEpoch.elapsed();
    EXPECT_NE(0, t1);
    auto t2 = bmcEpoch.elapsed();
    EXPECT_GE(t2, t1);
}


}
}
