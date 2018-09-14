#include "epoch_base.hpp"
#include "types.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

namespace phosphor
{
namespace time
{

class TestEpochBase : public testing::Test
{
  public:
    sdbusplus::bus::bus bus;
    EpochBase epochBase;

    TestEpochBase() : bus(sdbusplus::bus::new_default()), epochBase(bus, "")
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

    epochBase.onModeChanged(Mode::Manual);
    EXPECT_EQ(Mode::Manual, getMode());
}

TEST_F(TestEpochBase, onOwnerChange)
{
    epochBase.onOwnerChanged(Owner::BMC);
    EXPECT_EQ(Owner::BMC, getOwner());

    epochBase.onOwnerChanged(Owner::Host);
    EXPECT_EQ(Owner::Host, getOwner());

    epochBase.onOwnerChanged(Owner::Split);
    EXPECT_EQ(Owner::Split, getOwner());

    epochBase.onOwnerChanged(Owner::Both);
    EXPECT_EQ(Owner::Both, getOwner());
}

} // namespace time
} // namespace phosphor
