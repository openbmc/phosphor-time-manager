#include "config.h"

#include "bmc_epoch.hpp"
#include "manager.hpp"
#include "types.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

namespace phosphor
{
namespace time
{

class TestBmcEpoch : public testing::Test
{
  public:
    sdbusplus::bus_t bus;
    Manager manager;
    sd_event* event = nullptr;
    std::unique_ptr<BmcEpoch> bmcEpoch;

    TestBmcEpoch() : bus(sdbusplus::bus::new_default()), manager(bus)
    {
        // BmcEpoch requires sd_event to init
        sd_event_default(&event);
        bus.attach_event(event, SD_EVENT_PRIORITY_NORMAL);
        bmcEpoch = std::make_unique<BmcEpoch>(bus, objpathBmc, manager);
    }

    ~TestBmcEpoch() override
    {
        bus.detach_event();
        sd_event_unref(event);
    }
};

TEST_F(TestBmcEpoch, onModeChange)
{
    bmcEpoch->onModeChanged(Mode::NTP);
    EXPECT_EQ(Mode::NTP, manager.getTimeMode());

    bmcEpoch->onModeChanged(Mode::Manual);
    EXPECT_EQ(Mode::Manual, manager.getTimeMode());
}

TEST_F(TestBmcEpoch, empty)
{
    // Default mode is MANUAL
    EXPECT_EQ(Mode::Manual, manager.getTimeMode());
}

TEST_F(TestBmcEpoch, getElapsed)
{
    auto t1 = bmcEpoch->elapsed();
    EXPECT_NE(0, t1);
    auto t2 = bmcEpoch->elapsed();
    EXPECT_GE(t2, t1);
}

TEST_F(TestBmcEpoch, setElapsedOK)
{
    // TODO: setting time will call sd-bus functions and it will fail on host
    // if we have gmock for sdbusplus::bus, we can test setElapsed.
    // But for now we can not test it
}

} // namespace time
} // namespace phosphor
