#include <sdbusplus/bus.hpp>

#include "config.h"

int main()
{
    auto bus = sdbusplus::bus::new_default();

    bus.request_name(BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
