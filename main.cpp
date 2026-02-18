#include "config.h"

#include "bmc_epoch.hpp"
#include "manager.hpp"
#include "utils.hpp"

#include <getopt.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <string>

void usage(const char* prog)
{
    lg2::error("Usage: {PROG} [options]", "PROG", prog);
    lg2::error("Options:");
    lg2::error("  --mode=<ntp|manual>  Set initial time synchronization mode");
    lg2::error("                       ntp: Use NTP for time synchronization");
    lg2::error("                       manual: Use manual time setting");
    lg2::error("  -h, --help           Display this help message");
}

int main(int argc, char** argv)
{
    std::string initialMode;

    // Parse command line arguments
    static struct option long_options[] = {{"mode", required_argument, 0, 'm'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "h", long_options, &option_index)) !=
           -1)
    {
        switch (c)
        {
            case 'm':
                initialMode = optarg;
                if (initialMode != "ntp" && initialMode != "manual")
                {
                    lg2::error(
                        "Invalid mode '{MODE}'. Must be 'ntp' or 'manual'",
                        "MODE", initialMode);
                    usage(argv[0]);
                    return 1;
                }
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            case '?':
                usage(argv[0]);
                return 1;
            default:
                break;
        }
    }

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

    // Set initial mode if provided via command line
    if (!initialMode.empty())
    {
        phosphor::time::Mode mode;
        if (initialMode == "ntp")
        {
            mode = phosphor::time::Mode::NTP;
            lg2::info("Setting initial time sync mode to NTP");
        }
        else if (initialMode == "manual")
        {
            mode = phosphor::time::Mode::Manual;
            lg2::info("Setting initial time sync mode to Manual");
        }

        try
        {
            manager.setTimeMode(mode);
        }
        catch (const std::exception& e)
        {
            lg2::error("Failed to set initial mode: {ERROR}", "ERROR",
                       e.what());
            // Continue anyway - the manager will use default or persisted value
        }
    }

    bus.request_name(busname);

    // Start event loop for all sd-bus events and timer event
    sd_event_loop(bus.get_event());

    bus.detach_event();

    return 0;
}
