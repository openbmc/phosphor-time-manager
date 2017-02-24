#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "types.hpp"
#include "manager.hpp"
#include "mocked_property_change_listener.hpp"

using ::testing::_;

namespace phosphor
{
namespace time
{

class TestManager : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        Manager manager;
        MockPropertyChangeListner listener1;
        MockPropertyChangeListner listener2;

        TestManager()
            : bus(sdbusplus::bus::new_default()),
              manager(bus)
        {
            // Add two mocked listeners so that we can test
            // the behavior related to listeners
            manager.addListener(&listener1);
            manager.addListener(&listener2);
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

TEST_F(TestManager, propertyChanged)
{
    // When host is off, property change will be notified to listners
    EXPECT_FALSE(isHostOn());

    // Check mocked listeners shall receive notifications on property changed
    EXPECT_CALL(listener1, onModeChanged(Mode::MANUAL)).Times(1);
    EXPECT_CALL(listener1, onOwnerChanged(Owner::HOST)).Times(1);
    EXPECT_CALL(listener2, onModeChanged(Mode::MANUAL)).Times(1);
    EXPECT_CALL(listener2, onOwnerChanged(Owner::HOST)).Times(1);

    notifyPropertyChanged("time_mode", "MANUAL");
    notifyPropertyChanged("time_owner", "HOST");

    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());

    // When host is on, property changes are saved as requested ones
    notifyPgoodChanged(true);

    // Check mocked listeners shall not receive notifications
    EXPECT_CALL(listener1, onModeChanged(Mode::MANUAL)).Times(0);
    EXPECT_CALL(listener1, onOwnerChanged(Owner::HOST)).Times(0);
    EXPECT_CALL(listener2, onModeChanged(Mode::MANUAL)).Times(0);
    EXPECT_CALL(listener2, onOwnerChanged(Owner::HOST)).Times(0);

    notifyPropertyChanged("time_mode", "NTP");
    notifyPropertyChanged("time_owner", "SPLIT");

    EXPECT_EQ("NTP", getRequestedMode());
    EXPECT_EQ("SPLIT", getRequestedOwner());


    // When host becomes off, the requested mode/owner shall be notified
    // to listners, and be cleared
    EXPECT_CALL(listener1, onModeChanged(Mode::NTP)).Times(1);
    EXPECT_CALL(listener1, onOwnerChanged(Owner::SPLIT)).Times(1);
    EXPECT_CALL(listener2, onModeChanged(Mode::NTP)).Times(1);
    EXPECT_CALL(listener2, onOwnerChanged(Owner::SPLIT)).Times(1);

    notifyPgoodChanged(false);

    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());

    // When host is on, and invalid property is changed,
    // verify the code asserts because it shall never occur
    notifyPgoodChanged(true);
    ASSERT_DEATH(notifyPropertyChanged("invalid property", "whatever"), "");
}

TEST_F(TestManager, propertyChangedAndChangedbackWhenHostOn)
{
    // Property is now MANUAL/HOST
    notifyPropertyChanged("time_mode", "MANUAL");
    notifyPropertyChanged("time_owner", "HOST");

    // Set host on
    notifyPgoodChanged(true);

    // Check mocked listeners shall not receive notifications
    EXPECT_CALL(listener1, onModeChanged(_)).Times(0);
    EXPECT_CALL(listener1, onOwnerChanged(_)).Times(0);
    EXPECT_CALL(listener2, onModeChanged(_)).Times(0);
    EXPECT_CALL(listener2, onOwnerChanged(_)).Times(0);

    notifyPropertyChanged("time_mode", "NTP");
    notifyPropertyChanged("time_owner", "SPLIT");

    // Saved as requested mode/owner
    EXPECT_EQ("NTP", getRequestedMode());
    EXPECT_EQ("SPLIT", getRequestedOwner());

    // Property changed back to MANUAL/HOST
    notifyPropertyChanged("time_mode", "MANUAL");
    notifyPropertyChanged("time_owner", "HOST");

    // Requested mode/owner shall be updated
    EXPECT_EQ("MANUAL", getRequestedMode());
    EXPECT_EQ("HOST", getRequestedOwner());

    // Because the latest mode/owner is the same as when host is off,
    // The listeners shall not be notified, and requested mode/owner
    // shall be cleared
    EXPECT_CALL(listener1, onModeChanged(_)).Times(0);
    EXPECT_CALL(listener1, onOwnerChanged(_)).Times(0);
    EXPECT_CALL(listener2, onModeChanged(_)).Times(0);
    EXPECT_CALL(listener2, onOwnerChanged(_)).Times(0);

    notifyPgoodChanged(false);

    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());
}

// TODO: if gmock is ready, add case to test
// updateNtpSetting() and updateNetworkSetting()

}
}
