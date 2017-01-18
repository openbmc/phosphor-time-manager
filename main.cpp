#include <sdbusplus/bus.hpp>

#include "config.h"
#include "bmc_epoch.hpp"
#include "host_epoch.hpp"
#include "time_manager.hpp"

int main()
{
    auto bus = sdbusplus::bus::new_default();
    phosphor::time::Manager manager(bus);
    phosphor::time::BmcEpoch bmc(bus, OBJPATH_BMC, &manager);
    phosphor::time::HostEpoch host(bus,OBJPATH_HOST, &manager);

    bus.request_name(BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
