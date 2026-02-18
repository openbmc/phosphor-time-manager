#include "config.h"

#include "bmc_epoch.hpp"
#include "manager.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

int main(int argc, char** argv)
{
    CLI::App app{"phosphor-time-manager - OpenBMC Time Management Service"};

    std::string cmdlineMode;
    bool useCmdlineMode = false;

    // Add --mode option
    app.add_option(
           "--mode", cmdlineMode,
           "Run in specified time synchronization mode (ntp or manual)\n"
           "This mode will NOT update settings daemon and will ignore D-Bus property changes")
        ->check(CLI::IsMember({"ntp", "manual"}))
        ->group("Time Synchronization");

    // Parse command line
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        return app.exit(e);
    }

    // Check if mode was specified
    if (!cmdlineMode.empty())
    {
        useCmdlineMode = true;
        lg2::info("Command-line mode specified: {MODE}", "MODE", cmdlineMode);
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

    // Enable command-line override mode if specified
    if (useCmdlineMode)
    {
        phosphor::time::Mode mode = phosphor::time::Mode::Manual;
        if (cmdlineMode == "ntp")
        {
            mode = phosphor::time::Mode::NTP;
            lg2::info("Command-line mode: NTP");
        }
        else
        {
            mode = phosphor::time::Mode::Manual;
            lg2::info("Command-line mode: Manual");
        }

        manager.enableCmdlineOverride(mode);
    }
    else
    {
        lg2::info("Running in normal mode");
        lg2::info("Using settings from settings daemon");
    }

    bus.request_name(busname);

    // Start event loop for all sd-bus events and timer event
    sd_event_loop(bus.get_event());

    bus.detach_event();

    return 0;
}
