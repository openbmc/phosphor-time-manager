#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "epoch_base.hpp"

namespace phosphor
{
namespace time
{

class TestEpochBase : public testing::Test
{
    public:
        using Mode = EpochBase::Mode;
        using Owner = EpochBase::Owner;

        sdbusplus::bus::bus bus;
        Manager manager;
        EpochBase epochBase;

        TestEpochBase()
            : bus(sdbusplus::bus::new_default()),
              manager(bus),
              epochBase(bus, "", &manager)
        {
            // Empty
        }

        // Proxies for EpochBase's private members and functions
        Mode convertToMode(const std::string& mode)
        {
            return EpochBase::convertToMode(mode);
        }
        Owner convertToOwner(const std::string& owner)
        {
            return EpochBase::convertToOwner(owner);
        }
};

TEST_F(TestEpochBase, convertToMode)
{
    EXPECT_EQ(Mode::NTP, convertToMode("NTP"));
    EXPECT_EQ(Mode::MANUAL, convertToMode("MANUAL"));

    // All unrecognized strings are mapped to Ntp
    EXPECT_EQ(Mode::NTP, convertToMode(""));
    EXPECT_EQ(Mode::NTP, convertToMode("Manual"));
    EXPECT_EQ(Mode::NTP, convertToMode("whatever"));
}


TEST_F(TestEpochBase, convertToOwner)
{
    EXPECT_EQ(Owner::BMC, convertToOwner("BMC"));
    EXPECT_EQ(Owner::HOST, convertToOwner("HOST"));
    EXPECT_EQ(Owner::SPLIT, convertToOwner("SPLIT"));
    EXPECT_EQ(Owner::BOTH, convertToOwner("BOTH"));

    // All unrecognized strings are mapped to Bmc
    EXPECT_EQ(Owner::BMC, convertToOwner(""));
    EXPECT_EQ(Owner::BMC, convertToOwner("Split"));
    EXPECT_EQ(Owner::BMC, convertToOwner("xyz"));
}

}
}
