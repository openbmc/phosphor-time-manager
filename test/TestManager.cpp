#include "manager.hpp"
#include "types.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

namespace phosphor
{
namespace time
{

class TestManager : public testing::Test
{
  public:
    sdbusplus::bus_t bus;
    Manager manager;

    TestManager() : bus(sdbusplus::bus::new_default()), manager(bus) {}

    void notifyPropertyChanged(const std::string& key, const std::string& value)
    {
        manager.onPropertyChanged(key, value);
    }
};

TEST_F(TestManager, propertyChanged)
{
    notifyPropertyChanged(
        "TimeSyncMethod",
        "xyz.openbmc_project.Time.Synchronization.Method.Manual");
    EXPECT_EQ(Mode::Manual, manager.getTimeMode());

    notifyPropertyChanged(
        "TimeSyncMethod",
        "xyz.openbmc_project.Time.Synchronization.Method.NTP");
    EXPECT_EQ(Mode::NTP, manager.getTimeMode());

    // Invalid property should be ignored, mode stays unchanged
    notifyPropertyChanged("invalid property", "whatever");
    EXPECT_EQ(Mode::NTP, manager.getTimeMode());
}

} // namespace time
} // namespace phosphor
