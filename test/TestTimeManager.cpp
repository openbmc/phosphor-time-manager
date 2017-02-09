#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "manager.hpp"

namespace phosphor
{
namespace time
{

class TestTimeManager : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        Manager manager;

        TestTimeManager()
            : bus(sdbusplus::bus::new_default()),
              manager(bus)
        {
            // Empty
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

TEST_F(TestTimeManager, empty)
{
    EXPECT_FALSE(isHostOn());
    EXPECT_EQ("", getRequestedMode());
    EXPECT_EQ("", getRequestedOwner());
}

TEST_F(TestTimeManager, pgoodChange)
{
    notifyPgoodChanged(true);
    EXPECT_TRUE(isHostOn());
    notifyPgoodChanged(false);
    EXPECT_FALSE(isHostOn());
}

TEST_F(TestTimeManager, propertyChange)
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
