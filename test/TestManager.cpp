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
        bool isHostOn()
        {
            return manager.isHostOn;
        }
        std::string getRequestedMode()
        {
            return manager.requestedMode;
        }
        std::string getRequestedOwner()
        {
            return manager.requestedOwner;
        }
        void notifyPropertyChanged(const std::string& key,
                                   const std::string& value)
        {
            manager.onPropertyChanged(key, value);
        }
        void notifyPgoodChanged(bool pgood)
        {
            manager.onPgoodChanged(pgood);
        }
};

TEST_F(TestManager, empty)
{
    EXPECT_FALSE(isHostOn());
    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());
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

TEST_F(TestManager, pgoodChange)
{
    notifyPgoodChanged(true);
    EXPECT_TRUE(isHostOn());
    notifyPgoodChanged(false);
    EXPECT_FALSE(isHostOn());
}

TEST_F(TestManager, propertyChange)
{
    // When host is off, property change will be notified to listners
    EXPECT_FALSE(isHostOn());
    notifyPropertyChanged("time_mode", "MANUAL");
    notifyPropertyChanged("time_owner", "HOST");
    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());
    // TODO: if gmock is ready, check mocked listners shall receive notifies

    notifyPgoodChanged(true);
    // When host is on, property changes are saved as requested ones
    notifyPropertyChanged("time_mode", "MANUAL");
    notifyPropertyChanged("time_owner", "HOST");
    EXPECT_EQ("MANUAL", getRequestedMode());
    EXPECT_EQ("HOST", getRequestedOwner());


    // When host becomes off, the requested mode/owner shall be notified
    // to listners, and be cleared
    notifyPgoodChanged(false);
    // TODO: if gmock is ready, check mocked listners shall receive notifies
    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());

    // When host is on, and invalid property is changed,
    // verify the code asserts because it shall never occur
    notifyPgoodChanged(true);
    ASSERT_DEATH(notifyPropertyChanged("invalid property", "whatever"), "");
}

}
}
