#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "bmc_epoch.hpp"
#include "config.h"

namespace phosphor
{
namespace time
{

using namespace std::chrono;
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
        void setTimeOwner(Owner owner)
        {
            bmcEpoch.timeOwner = owner;
        }
        void setTimeMode(Mode mode)
        {
            bmcEpoch.timeMode = mode;
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

TEST_F(TestBmcEpoch, setElapsedNotAllowed)
{
    auto epochNow = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
    // In NTP mode, setting time is not allowed
    auto ret = bmcEpoch.elapsed(epochNow);
    EXPECT_EQ(0, ret);

    // In Host owner, setting time is not allowed
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::HOST);
    ret = bmcEpoch.elapsed(epochNow);
    EXPECT_EQ(0, ret);
}

TEST_F(TestBmcEpoch, setElapsedOK)
{
    // TODO: setting time will call sd-bus functions and it will fail on host
    // if we have gmock for sdbusplus::bus, we can test setElapsed.
    // But for now we can not test it
}

}
}
