#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "types.hpp"
#include "manager.hpp"

namespace phosphor
{
namespace time
{

class TestManager : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        Manager manager;

        TestManager()
            : bus(sdbusplus::bus::new_default()),
              manager(bus)
        {
            // Empty
        }

        // Proxies for Manager's private members and functions
         Mode getTimeMode()
        {
            return manager.timeMode;
        }
        Owner getTimeOwner()
        {
            return manager.timeOwner;
        }
        Mode convertToMode(const std::string& mode)
        {
            return Manager::convertToMode(mode);
        }
        Owner convertToOwner(const std::string& owner)
        {
            return Manager::convertToOwner(owner);
        }
};

TEST_F(TestManager, empty)
{
    EXPECT_EQ(Mode::NTP, getTimeMode());
    EXPECT_EQ(Owner::BMC, getTimeOwner());
}

TEST_F(TestManager, convertToMode)
{
    EXPECT_EQ(Mode::NTP, convertToMode("NTP"));
    EXPECT_EQ(Mode::MANUAL, convertToMode("MANUAL"));

    // All unrecognized strings are mapped to Ntp
    EXPECT_EQ(Mode::NTP, convertToMode(""));
    EXPECT_EQ(Mode::NTP, convertToMode("Manual"));
    EXPECT_EQ(Mode::NTP, convertToMode("whatever"));
}


TEST_F(TestManager, convertToOwner)
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
