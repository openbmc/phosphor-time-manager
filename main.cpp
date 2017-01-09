#include <sdbusplus/bus.hpp>

#include "config.h"
#include "time_manager.hpp"

int main()
{
    auto bus = sdbusplus::bus::new_default();
    phosphor::time::Manager manager(bus, BUSNAME, OBJPATH);

    bus.request_name(BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
