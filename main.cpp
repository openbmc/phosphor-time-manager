#include <sdbusplus/bus.hpp>

#include "config.h"
#include "bmc_epoch.hpp"
#include "host_epoch.hpp"
#include "manager.hpp"

int main()
{
    auto bus = sdbusplus::bus::new_default();
    sd_event* event = nullptr;

    // acquire a referece to the default event loop
    sd_event_default(&event);

    // attach bus to this event loop
    bus.attach_event(event, SD_EVENT_PRIORITY_NORMAL);

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

    // Start event loop for all sd-bus events and timer event
    sd_event_loop(bus.get_event());

    bus.detach_event();
    sd_event_unref(event);

    return 0;
}
