#include <sdbusplus/bus.hpp>

#include "config.h"
#include "bmc_epoch.hpp"

int main()
{
    auto bus = sdbusplus::bus::new_default();
    phosphor::time::BmcEpoch bmc(bus, OBJPATH_BMC);

    bus.request_name(BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
