#include "config.h"

#include "bmc_epoch.hpp"
#include "manager.hpp"

#include <sdbusplus/bus.hpp>

int main()
{
    auto bus = sdbusplus::bus::new_default();
    sd_event* event = nullptr;

    auto eventDeleter = [](sd_event* e) { sd_event_unref(e); };
    using SdEvent = std::unique_ptr<sd_event, decltype(eventDeleter)>;

    // acquire a reference to the default event loop
    sd_event_default(&event);
    SdEvent sdEvent{event, eventDeleter};
    event = nullptr;

    // attach bus to this event loop
    bus.attach_event(sdEvent.get(), SD_EVENT_PRIORITY_NORMAL);

    // Add sdbusplus ObjectManager
    sdbusplus::server::manager_t bmcEpochObjManager(bus, objmgrpath);

    phosphor::time::Manager manager(bus);
    phosphor::time::BmcEpoch bmc(bus, objpathBmc, manager);

    bus.request_name(busname);

    // Start event loop for all sd-bus events and timer event
    sd_event_loop(bus.get_event());

    bus.detach_event();

    return 0;
}
