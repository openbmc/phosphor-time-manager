#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "types.hpp"
#include "epoch_base.hpp"

namespace phosphor
{
namespace time
{

class TestEpochBase : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        EpochBase epochBase;

        TestEpochBase()
            : bus(sdbusplus::bus::new_default()),
              epochBase(bus, "")
        {
            // Empty
        }

        Mode getMode()
        {
            return epochBase.timeMode;
        }
        Owner getOwner()
        {
            return epochBase.timeOwner;
        }
};

TEST_F(TestEpochBase, onModeChange)
{
    epochBase.onModeChanged(Mode::NTP);
    EXPECT_EQ(Mode::NTP, getMode());

    epochBase.onModeChanged(Mode::MANUAL);
    EXPECT_EQ(Mode::MANUAL, getMode());
}

TEST_F(TestEpochBase, onOwnerChange)
{
    epochBase.onOwnerChanged(Owner::BMC);
    EXPECT_EQ(Owner::BMC, getOwner());

    epochBase.onOwnerChanged(Owner::HOST);
    EXPECT_EQ(Owner::HOST, getOwner());

    epochBase.onOwnerChanged(Owner::SPLIT);
    EXPECT_EQ(Owner::SPLIT, getOwner());

    epochBase.onOwnerChanged(Owner::BOTH);
    EXPECT_EQ(Owner::BOTH, getOwner());
}

}
}
