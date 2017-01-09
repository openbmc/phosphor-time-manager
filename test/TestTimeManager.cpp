#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "time_manager.hpp"
#include "config.h"

class TestTimeManager : public testing::Test
{
public:
    using Manager = phosphor::time::Manager;

    sdbusplus::bus::bus bus;
    Manager timeManager;


    TestTimeManager()
     : bus(sdbusplus::bus::new_default()),
       timeManager(bus, BUSNAME, OBJPATH)
    {
        // Empty
    }
};

TEST_F(TestTimeManager, empty)
{
}
