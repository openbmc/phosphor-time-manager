#include <sdbusplus/bus.hpp>

#include "config.h"
#include "bmc_epoch.hpp"
#include "host_epoch.hpp"
#include "manager.hpp"

int main()
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager
    sdbusplus::server::manager::manager bmcEpochObjManager(bus, OBJPATH_BMC);
    sdbusplus::server::manager::manager hostEpochObjManager(bus, OBJPATH_HOST);

    phosphor::time::Manager manager(bus);
    phosphor::time::BmcEpoch bmc(bus, OBJPATH_BMC);
    phosphor::time::HostEpoch host(bus,OBJPATH_HOST);

    manager.addListener(&bmc);
    manager.addListener(&host);
    bmc.setBcmTimeChangeListener(&host);

    bus.request_name(BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
