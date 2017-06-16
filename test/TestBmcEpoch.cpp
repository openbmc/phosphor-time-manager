#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>
#include <memory>

#include "bmc_epoch.hpp"
#include "config.h"
#include "types.hpp"
#include "mocked_bmc_time_change_listener.hpp"

namespace phosphor
{
namespace time
{

using ::testing::_;
using namespace std::chrono;

class TestBmcEpoch : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        sd_event* event;
        MockBmcTimeChangeListener listener;
        std::unique_ptr<BmcEpoch> bmcEpoch;

        TestBmcEpoch()
            : bus(sdbusplus::bus::new_default())
        {
            // BmcEpoch requires sd_event to init
            sd_event_default(&event);
            bus.attach_event(event, SD_EVENT_PRIORITY_NORMAL);
            bmcEpoch = std::make_unique<BmcEpoch>(bus, OBJPATH_BMC);
            bmcEpoch->setBmcTimeChangeListener(&listener);
        }

        ~TestBmcEpoch()
        {
            bus.detach_event();
            sd_event_unref(event);
        }

        // Proxies for BmcEpoch's private members and functions
        Mode getTimeMode()
        {
            return bmcEpoch->timeMode;
        }
        Owner getTimeOwner()
        {
            return bmcEpoch->timeOwner;
        }
        void setTimeOwner(Owner owner)
        {
            bmcEpoch->timeOwner = owner;
        }
        void setTimeMode(Mode mode)
        {
            bmcEpoch->timeMode = mode;
        }
        void triggerTimeChange()
        {
            bmcEpoch->onTimeChange(nullptr,
                                   -1,
                                   0,
                                   bmcEpoch.get());
        }
};

TEST_F(TestBmcEpoch, empty)
{
    // Default mode/owner is MANUAL/BOTH
    EXPECT_EQ(Mode::MANUAL, getTimeMode());
    EXPECT_EQ(Owner::BOTH, getTimeOwner());
}

TEST_F(TestBmcEpoch, getElapsed)
{
    auto t1 = bmcEpoch->elapsed();
    EXPECT_NE(0, t1);
    auto t2 = bmcEpoch->elapsed();
    EXPECT_GE(t2, t1);
}

TEST_F(TestBmcEpoch, setElapsedNotAllowed)
{
    setTimeMode(Mode::NTP);
    auto epochNow = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()).count();
    // In NTP mode, setting time is not allowed
    auto ret = bmcEpoch->elapsed(epochNow);
    EXPECT_EQ(0, ret);

    // In Host owner, setting time is not allowed
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::HOST);
    ret = bmcEpoch->elapsed(epochNow);
    EXPECT_EQ(0, ret);
}

TEST_F(TestBmcEpoch, setElapsedOK)
{
    // TODO: setting time will call sd-bus functions and it will fail on host
    // if we have gmock for sdbusplus::bus, we can test setElapsed.
    // But for now we can not test it
}

TEST_F(TestBmcEpoch, onTimeChange)
{
    // On BMC time change, the listner is expected to be notified
    EXPECT_CALL(listener, onBmcTimeChanged(_)).Times(1);
    triggerTimeChange();
}

}
}
