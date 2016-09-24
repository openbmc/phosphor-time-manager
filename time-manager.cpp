#define _XOPEN_SOURCE
#include <chrono>
#include <sstream>
#include <iomanip>
#include <array>
#include <sys/timerfd.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include "time-utils.hpp"
#include "time-manager.hpp"

// Neeed to do this since its not exported outside of the kernel.
// Refer : https://gist.github.com/lethean/446cea944b7441228298
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

// Needed to make sure timerfd does not misfire eventhough we set CANCEL_ON_SET
#define TIME_T_MAX (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

constexpr auto WATCH_SETTING_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

constexpr auto WATCH_PGOOD_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/control/power0',member='PropertiesChanged'";

sd_bus* gTimeBus = nullptr;
sd_bus_slot* gTimeSlot = nullptr;

// Globals to support multiple modes and owners
timeModes  gCurrTimeMode = timeModes::NTP;
timeModes  gRequestedTimeMode = timeModes::NTP;

timeOwners gCurrTimeOwner = timeOwners::BMC;
timeOwners gRequestedTimeOwner = timeOwners::BMC;

// Used when owner is 'SPLIT'
std::chrono::microseconds gHostOffset(0);
std::chrono::microseconds gUptimeUsec(0);

std::string gCurrDhcpNtp = "yes";
bool gTimeSettingsChangesAllowed = false;

// Used in time-register.c
extern sd_bus_vtable timeServicesVtable[];

// OpenBMC Time Manager dbus framework
constexpr auto timeBusName =  "org.openbmc.TimeManager";
constexpr auto timeObjPath  =  "/org/openbmc/TimeManager";
constexpr auto timeIntfName =  "org.openbmc.TimeManager";

// Interested time parameters
timeParamsMap gTimeParams;

// Reads timeofday and returns time string
// and also number of microseconds.
// Ex : Tue Aug 16 22:49:43 2016
int getBmcTime(sd_bus_message* m, void* userdata,
               sd_bus_error* retError)
{
    std::cout << "Request to get BMC time" << std::endl;

    auto currBmcTime = std::chrono::system_clock::now();
    auto bmcTime_t = std::chrono::system_clock::to_time_t(currBmcTime);

    auto timeInUsec = std::chrono::duration_cast<std::chrono::microseconds>
                   (currBmcTime.time_since_epoch());

    std::ostringstream timeFormat {};
    timeFormat << std::put_time(std::gmtime(&bmcTime_t), "%c %Z");

    return sd_bus_reply_method_return(m, "xs",
                 (int64_t)timeInUsec.count(), timeFormat.str().c_str());
}

// Designated to be called by IPMI_GET_SEL time from host
int getHostTime(sd_bus_message* m, void* userdata,
                sd_bus_error* retError)
{
    using namespace std::chrono;

    std::cout << "Request to get HOST time" << std::endl;

    auto timeInUsec = duration_cast<microseconds>
                      (system_clock::now().time_since_epoch());

    auto hostTime = timeInUsec + gHostOffset;
    auto timeInSec = std::chrono::duration_cast<std::chrono::seconds>
                     (std::chrono::microseconds(timeInUsec)).count();

    std::cout << " host_time:[ " << hostTime.count()
              << " ] host_offset: [ " << gHostOffset.count()
              << " ] TIME In SEC: [ " << timeInSec << " ] " << std::endl;

    return sd_bus_reply_method_return(m, "x", (uint64_t)hostTime.count());
}

// Helper function to set the time.
auto setTimeOfDay(const std::chrono::microseconds timeOfDayUsec)
{
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // These 2 are for bypassing some policy
    // checking in the timedate1 service
    auto relative = false;
    auto interactive = false;

    auto r = sd_bus_call_method(gTimeBus,
                                "org.freedesktop.timedate1",
                                "/org/freedesktop/timedate1",
                                "org.freedesktop.timedate1",
                                "SetTime",
                                &busError,
                                nullptr,            // timedate1 does not return response
                                "xbb",
                                (int64_t)timeOfDayUsec.count(), //newTimeUsec,
                                relative,        // Time in absolute seconds since epoch
                                interactive);    // bypass polkit checks

    sd_bus_error_free(&busError);
    return r;
}

// Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
// and then sets the BMC time. If the input time string does not conform to the
// format, an error message is returned.
int setBmcTime(sd_bus_message* m, void* userdata,
               sd_bus_error* retError)
{
    tm userTm {};
    std::time_t timeOfDay = 0;
    std::chrono::microseconds timeInUsec(0);
    const char* userTimeStr = nullptr;
    char* timeVal = nullptr;
    std::istringstream timeString {};
    int r = -1;

    std::cout << "Request to set BMC time" << std::endl;

    std::cout << "Curr_Mode: " << modeStr(gCurrTimeMode)
              << " Curr_Owner: " << ownerStr(gCurrTimeOwner)
              << std::endl;

    if (gCurrTimeMode == timeModes::NTP ||
            gCurrTimeOwner == timeOwners::HOST)
    {
        std::cerr << "Can not set time" << std::endl;
        goto finish;
    }

    r = sd_bus_message_read(m, "s", &userTimeStr);
    if (r < 0)
    {
        std::cerr << "Error:" << strerror(-r)
                  <<" reading user time" << std::endl;
        goto finish;
    }

    timeString.str(userTimeStr);
    timeString >> std::get_time(&userTm, "%Y-%m-%d %H:%M:%S");
    if (timeString.fail())
    {
        r = -1;
        std::cerr << "Error: Incorrect time format" << std::endl;
        goto finish;
    }

    // Convert the time structure into number of
    // seconds maintained in GMT. Followed the same that is in
    // systemd/timedate1
    timeOfDay = timegm(&userTm);
    if (timeOfDay < 0)
    {
        r = -1;
        std::cerr <<"Error converting tm to seconds" << std::endl;
        goto finish;
    }

    // Set REALTIME and also update hwclock
    timeInUsec = std::chrono::microseconds(
                std::chrono::seconds(timeOfDay));
    r = setTimeOfDay(timeInUsec);
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  << "setting time on BMC" << std::endl;
    }
finish:
    return sd_bus_reply_method_return(m, "i", (r < 0 ) ? r : 0);
}

// Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
// and then sets the BMC time. If the input time string does not conform to the
// format, an error message is returned.
int setHostTime(sd_bus_message* m, void* userdata,
                sd_bus_error* retError)
{
    using namespace std::chrono;

    std::cout << "Request to SET Host time" << std::endl;

    // We should not return failure to host just becase
    // policy does not allow it
    int r = 0;
    uint64_t newHostTime {};
    microseconds hostTimeUsec(0);

    std::cout << "Curr_Mode: " << modeStr(gCurrTimeMode)
              << "Curr_Owner: " << ownerStr(gCurrTimeOwner)
              << "host_offset: " << gHostOffset.count() << std::endl;

    if (gCurrTimeOwner == timeOwners::BMC)
    {
        goto finish;
    }

    r = sd_bus_message_read(m, "x", &newHostTime);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "reading host time" << std::endl;
        goto finish;
    }
    std::cout << "setHostTime: newHostTime: "
              << newHostTime << std::endl;

    // Convert the Host Time to chrono::duration
    hostTimeUsec = microseconds(microseconds(newHostTime));

    if (gCurrTimeOwner == timeOwners::SPLIT)
    {
        // Adjust the host offset
        auto bmcTimeUsec = std::chrono::duration_cast<std::chrono::microseconds>
                           (std::chrono::system_clock::now().time_since_epoch());

        gHostOffset = bmcTimeUsec - hostTimeUsec;
        r = writeData<uint64_t>(hostOffsetFile, gHostOffset.count());
        if (r < 0)
        {
            std::cerr << "Error saving host_offset: "
                      << gHostOffset.count() << std::endl;
        }
        // We are not doing any time settings in BMC
        std::cout << "Updated: host_time: [  " << newHostTime
                  << " ] host_offset: " << gHostOffset.count()
                  << " ] " << std::endl;

        goto finish;
    }

    // We are okay to update time in as long as BMC is not the owner
    r = setTimeOfDay(hostTimeUsec);

finish:
    return sd_bus_reply_method_return(m, "i", (r < 0) ? r : 0);
}

// Property reader
int getTimeManagerProperty(sd_bus* bus, const char* path,
                           const char* interface, const char* property,
                           sd_bus_message* m, void* userdata,
                           sd_bus_error* error)
{
    if (!strcmp(property, "curr_time_mode"))
    {
        return sd_bus_message_append(m, "s", modeStr(gCurrTimeMode).c_str());
    }
    else if (!strcmp(property, "curr_time_owner"))
    {
        return sd_bus_message_append(m, "s", ownerStr(gCurrTimeOwner).c_str());
    }
    else if (!strcmp(property, "requested_time_mode"))
    {
        return sd_bus_message_append(m, "s", modeStr(gRequestedTimeMode).c_str());
    }
    else if (!strcmp(property, "requested_time_owner"))
    {
        return sd_bus_message_append(m, "s", ownerStr(gRequestedTimeOwner).c_str());
    }
    return -EINVAL;
}

// Gets called into  by sd_event on an activity seen on sd_bus
int processSdBusMessage(sd_event_source* es, int fd,
                        uint32_t revents, void* userdata)
{
    int r = sd_bus_process(gTimeBus, nullptr);
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  <<" processing sd_bus message:" << std::endl;
    }
    return r;
}

// Gets called into by sd_event on any time SET event
int processTimeChange(sd_event_source* es, int fd,
                      uint32_t revents, void* userdata)
{
    using namespace std::chrono;
    std::array<char, 64> time {};

    std::cout << "BMC time changed" << std::endl;
    std::cout <<" Curr_Mode: " << modeStr(gCurrTimeMode)
              << " Curr_Owner: " << ownerStr(gCurrTimeOwner)
              << " Host_Offset: " << gHostOffset.count() << std::endl;

    // We are not interested in the data here. Need to read time again .
    // So read until there is something here in the FD
    while (read(fd, time.data(), time.max_size()) > 0);

    // Read the current BMC time and adjust the
    // host time offset if the mode is  SPLIT
    if (gCurrTimeOwner == timeOwners::SPLIT)
    {
        // Delta between REAL and Monotonic.
        auto uptimeUsec = duration_cast<microseconds>
                            (system_clock::now().time_since_epoch() -
                             steady_clock::now().time_since_epoch());

        auto deltaTimeUsec = uptimeUsec - gUptimeUsec;
        gHostOffset += deltaTimeUsec;
        gUptimeUsec = deltaTimeUsec;

        // Persist this
        auto r = writeData<uint64_t>(hostOffsetFile, gHostOffset.count());
        if (r < 0)
        {
            std::cerr << "Error saving host_offset: "
                      << gHostOffset.count() << std::endl;
            return r;
        }
        std::cout << " Updated: Host_Offset: "
                  << gHostOffset.count() << std::endl;
    }
    return 0;
}

// Accepts the time mode and makes necessary changes to timedate1
int modifyNtpSettings(const timeModes newTimeMode)
{
    auto ntpChangeOp = 0;

    // Error return mechanism
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // Pass '1' -or- '0' to SetNTP method indicating Enable/Disable
    ntpChangeOp = (newTimeMode == timeModes::NTP) ? 1 : 0;

    std::cout <<"Applying NTP setting..." << std::endl;

    auto r = sd_bus_call_method(gTimeBus,
                           "org.freedesktop.timedate1",
                           "/org/freedesktop/timedate1",
                           "org.freedesktop.timedate1",
                           "SetNTP",
                           &busError,
                           nullptr,    // timedate1 does not return response
                           "bb",
                           ntpChangeOp,       // '1' means Enable
                           0);              // '0' meaning no policy-kit
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  << "changing time Mode" << std::endl;
    }
    else
    {
        std::cout << "SUCCESS. NTP setting is now: " <<
                  (ntpChangeOp) ? "Enabled" : "Disabled";

        if (ntpChangeOp)
        {
            system("systemctl restart systemd-timesyncd &");
        }
        else
        {
            system("systemctl stop systemd-timesyncd &");
        }
    }
    return r;
}

// Manipulates time owner if the system setting allows it
int updateTimeMode(std::string newModeStr)
{
    auto r = 0;
    gRequestedTimeMode = getTimeMode(newModeStr);

    std::cout <<"Requested_Mode: " << newModeStr
              << " Current_Mode: " << modeStr(gCurrTimeMode)
              << std::endl;

    if (gRequestedTimeMode == gCurrTimeMode)
    {
        std::cout << "Mode is already set to : "
                  << newModeStr << std::endl;
        return 0;
    }

    // Also, if the time owner is HOST, then we should not allow NTP.
    // However, it may so happen that there are 2 pending requests, one for
    // changing to NTP and other for changing owner to something not HOST.
    // So check if there is a pending timeOwner change and if so, allow NTP
    // if the current is HOST and requested is non HOST.
    if (gCurrTimeOwner == timeOwners::HOST &&
            gRequestedTimeOwner == timeOwners::HOST &&
            gRequestedTimeMode == timeModes::NTP)
    {
        std::cout <<"Can not set mode to NTP with HOST as owner"
                  << std::endl;
        return 0;
    }

    if (gTimeSettingsChangesAllowed)
    {
        r = modifyNtpSettings(gRequestedTimeMode);
        if (r < 0)
        {
            std::cerr << "Error changing TimeMode settings"
                      << std::endl;
        }
        else
        {
            gCurrTimeMode = gRequestedTimeMode;
        }
        std::cout << "Current_Mode changed to: "
                  << newModeStr << std::endl;

        // Need this when we either restart or come back from reset
        r = writeData<std::string>(timeModeFile, modeStr(gCurrTimeMode));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it"
                  << std::endl;
    }
    return r;
}

// Manipulates time owner if the system setting allows it
int updateTimeOwner(const std::string newOwnerStr)
{
    int r = 0;
    gRequestedTimeOwner = getTimeOwner(newOwnerStr);

    // Needed when owner changes to HOST
    std::string manualMode = "Manual";

    if (gRequestedTimeOwner == gCurrTimeOwner)
    {
        std::cout <<"Owner is already set to : "
                  << newOwnerStr << std::endl;
        return 0;
    }

    std::cout <<"Requested_Owner: " << newOwnerStr
              << " Current_Owner: " << ownerStr(gCurrTimeOwner)
              << std::endl;

    if (gTimeSettingsChangesAllowed)
    {
        // If we are transitioning from SPLIT to something else,
        // reset the host offset.
        if (gCurrTimeOwner == timeOwners::SPLIT &&
                gRequestedTimeOwner != timeOwners::SPLIT)
        {
            gHostOffset = std::chrono::microseconds(0);
            writeData<uint64_t>(hostOffsetFile, gHostOffset.count());
            if (r < 0)
            {
                std::cerr << "Error saving host_offset: "
                          << gHostOffset.count() << std::endl;
            }
        }
        gCurrTimeOwner = gRequestedTimeOwner;
        std::cout << "Current_Owner is now: "
                  << newOwnerStr << std::endl;

        // HOST and NTP are exclusive
        if (gCurrTimeOwner == timeOwners::HOST)
        {
            std::cout <<"Forcing the mode to MANUAL" << std::endl;
            r = updateTimeMode(manualMode);
            if (r < 0)
            {
                std::cerr << "Error forcing the mode to MANUAL" << std::endl;
                return r;
            }
        }

        // Need this when we either restart or come back from reset
        r = writeData<std::string>(timeOwnerFile, ownerStr(gCurrTimeOwner));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it" << std::endl;
    }

    return r;
}

// Called by sd_event when Properties are changed in Control/power0
// Interested in change to 'pgood'
int processPgoodChange(std::string newPgood)
{
    int r = 0;

    // Indicating that we are safe to apply any changes
    if (!newPgood.compare("0"))
    {
        gTimeSettingsChangesAllowed = true;
        std::cout <<"Changing time settings allowed now" << std::endl;
    }
    else
    {
        gTimeSettingsChangesAllowed = false;
        std::cout <<"Changing time settings is *NOT* allowed now" << std::endl;
    }

    // if we have had users that changed the time settings
    // when we were not ready yet, do it now.
    if (gRequestedTimeOwner != gCurrTimeOwner)
    {
        r = updateTimeOwner(ownerStr(gRequestedTimeOwner));
        if (r < 0)
        {
            std::cerr << "Error updating new time owner" << std::endl;
            return r;
        }
        std::cout << "New Owner is : "
                  << ownerStr(gRequestedTimeOwner) << std::endl;
    }

    if (gRequestedTimeMode != gCurrTimeMode)
    {
        r = updateTimeMode(modeStr(gRequestedTimeMode));
        if (r < 0)
        {
            std::cerr << "Error updating new time mode" << std::endl;
            return r;
        }
        std::cout <<"New Mode is : "
                  << modeStr(gRequestedTimeMode) << std::endl;
    }
    return 0;
}

// This is called by Property Change handler on the event of
// receiving notification on property value change.
auto updatePropertyVal(std::string key, std::string value)
{
    for (auto& iter : gTimeParams)
    {
        if (!strcasecmp(key.c_str(), iter.first.first.c_str()))
        {
            return iter.second.second(value);
        }
    }
    return 0;
}

// Called by sd_event when Properties are changed in settingsd.
// Interested in changes to 'timeMode', 'timeOwner' and 'use_dhcp_ntp'
int processPropertyChange(sd_bus_message* m, void* user_data,
                          sd_bus_error* retError)
{
    const char* key = nullptr;
    const char* value = nullptr;

    auto newPgood = -1;
    auto r = -1;

    std::cout <<" User Settings have changed.." << std::endl;

    // input data is "sa{sv}as" and we are just interested in a{sv}
    r = sd_bus_message_skip(m, "s");
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r) <<
                  "skipping interface name in data" << std::endl;
        goto finish;
    }

    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<"entering the dictionary" << std::endl;
        goto finish;
    }

    while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY,
                "sv")) > 0)
    {
        r = sd_bus_message_read(m, "s", &key);
        if (r < 0)
        {
            std::cerr << "Error: " << strerror(-r)
                      <<" reading the key from dict" << std::endl;
            // Can not continue here since the next
            // enter would result in error anyway
            goto finish;
        }

        if (!strcmp(key, "time_mode"))
        {
            r = sd_bus_message_read(m, "v", "s", &value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "reading timeMode" << std::endl;
                goto finish;
            }
            updatePropertyVal(key, value);
        }
        else if (!strcmp(key, "time_owner"))
        {
            r = sd_bus_message_read(m, "v", "s", &value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "reading timeOwner" << std::endl;
                goto finish;
            }
            updatePropertyVal(key, value);
        }
        else if (!strcmp(key, "use_dhcp_ntp"))
        {
            r = sd_bus_message_read(m, "v", "s", &value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          <<"reading use_dhcp_ntp" << std::endl;
                goto finish;
            }
            updatePropertyVal(key, value);
        }
        else if (!strcmp(key, "pgood"))
        {
            r = sd_bus_message_read(m, "v", "i", &newPgood);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "reading pgood" << std::endl;
                goto finish;
            }
            updatePropertyVal(key, std::to_string(newPgood));
        }
        else
        {
            sd_bus_message_skip(m, "v");
        }
    }
finish:
    return r;
}

// Reads the values from 'settingsd' and applies:
// 1) Time Mode
// 2) time Owner
// 3) UseNTP setting
// 4) Pgood
auto processInitialSettings(void)
{
    // Read saved info like Host Offset, Who was the owner , what was the mode,
    // what was the use_dhcp_ntp setting before etc..
    auto r = readPersistentData();
    if (r < 0)
    {
        std::cerr << "Error reading the data saved in flash."
                  << std::endl;
        return r;
    }

    // Now read whats in settings and apply if allowed.
    for (auto& iter : gTimeParams)
    {
        // Get the settings value for the various keys.
        auto value = iter.first.second(iter.first.first);
        if (!value.empty())
        {
            std::cout << value;
            // Get the value for the key and validate.
            iter.second.first = value;
            int r =  iter.second.second(value);
            if (r < 0)
            {
                std::cerr << "Error setting up initial keys" << std::endl;
                return r;
            }
        }
        else
        {
            std::cerr << "key with no value: "
                      << iter.first.first << std::endl;
            return -1;
        }
    }

    // Now that we have taken care of consuming, push this as well
    // so that we can use the same map for handling pgood change too.
    //std::string value {};
    keyReader get = std::make_pair("pgood", &getPowerSetting);
    valueUpdater set = std::make_pair("", &processPgoodChange);
    gTimeParams.emplace(get, set);

    return 0;
}

// Sets up callback handlers for activities on :
// 1) user request on SD_BUS
// 2) Time change
// 3) Settings change
// 4) System state change;
auto registerCallbackHandlers(sd_event* event, sd_event_source* eventSource)
{
    auto sdBusFd = -1;
    auto timeFd = -1;
    auto r = -1;

    // Extract the descriptor out of sd_bus construct.
    sdBusFd = sd_bus_get_fd(gTimeBus);

    r = sd_event_add_io(event, &eventSource, sdBusFd, EPOLLIN,
                        processSdBusMessage, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<" adding sd_bus_message handler" << std::endl;
        return r;
    }

    // Choose the MAX time that is possible to aviod mis fires.
    itimerspec maxTime {};
    maxTime.it_value.tv_sec = TIME_T_MAX;

    timeFd = timerfd_create(CLOCK_REALTIME, 0);
    if (timeFd < 0)
    {
        std::cerr << "Errorno: " << errno << " creating timerfd" << std::endl;
        return -1;
    }

    r = timerfd_settime(timeFd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET,
                        &maxTime, nullptr);
    if (r)
    {
        std::cerr << "Errorno: " << errno << "Setting timerfd" << std::endl;
        return -1;
    }

    // Wake me up *only* if someone SETS the time
    r = sd_event_add_io(event, &eventSource, timeFd, EPOLLIN,
                        processTimeChange, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "adding time_change handler" << std::endl;
        return r;
    }

    // Watch for property changes in settingsd
    r = sd_bus_add_match(gTimeBus, &gTimeSlot, WATCH_SETTING_CHANGE,
                         processPropertyChange, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<" adding property change listener" << std::endl;
        return r;
    }

    // Watch for state change. Only reliable one to count on is
    // state of [pgood]. value of [1] meaning host is powering on / powered
    // on. [0] means powered off.
    r = sd_bus_add_match(gTimeBus, &gTimeSlot, WATCH_PGOOD_CHANGE,
                         processPropertyChange, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << " adding pgood change listener" << std::endl;
    }
    return r;
}

// Reads all the saved data from the last run
int readPersistentData()
{
    using namespace std::chrono;

    // Get current host_offset
    auto hostTimeOffset = readData<uint64_t>(hostOffsetFile);
    if (hostTimeOffset < 0)
    {
        std::cerr <<"Error reading current host offset"
                  << std::endl;
        return -1;
    }
    gHostOffset = std::chrono::microseconds(hostTimeOffset);
    std::cout <<"Last known host_offset:" << hostTimeOffset << std::endl;

    //How long was the FSP up prior to 'this' start
    gUptimeUsec = duration_cast<microseconds>
                (system_clock::now().time_since_epoch() -
                 steady_clock::now().time_since_epoch());
    if (gUptimeUsec.count() < 0)
    {
        std::cerr <<"Error reading uptime" << std::endl;
        return -1;
    }
    std::cout <<"Delta Time Usec: " << gUptimeUsec.count() << std::endl;

    // If we are coming back from a reset reload, then need to
    // read what was the last successful Mode and Owner.
    auto savedTimeMode = readData<std::string>(timeModeFile);
    if (!savedTimeMode.empty())
    {
        gCurrTimeMode = getTimeMode(savedTimeMode);
        if (gCurrTimeMode == timeModes::INVALID)
        {
            std::cerr << "INVALID mode returned" << std::endl;
            return -1;
        }
    }
    std::cout <<"Last known time_mode:"
              << savedTimeMode.c_str() << std::endl;

    auto savedTimeOwner = readData<std::string>(timeOwnerFile);
    if (!savedTimeOwner.empty())
    {
        gCurrTimeOwner = getTimeOwner(savedTimeOwner);
        if (gCurrTimeOwner == timeOwners::INVALID)
        {
            std::cerr << "INVALID Owner returned" << std::endl;
            return -1;
        }
    }
    std::cout <<"Last known time_owner: "
              << savedTimeOwner.c_str() << std::endl;

    auto savedDhcpNtp = readData<std::string>(dhcpNtpFile);
    if (!savedDhcpNtp.empty())
    {
        gCurrDhcpNtp = savedDhcpNtp;
    }
    std::cout <<"Last known use_dhcp_ntp: "
              << gCurrDhcpNtp.c_str() << std::endl;

    // Doing this here to make sure 'pgood' is read and handled
    // first before anything.
    auto pgood = getPowerSetting("pgood");
    if (!pgood.compare("0"))
    {
        std::cout << "Changing settings *allowed* now" << std::endl;
        gTimeSettingsChangesAllowed = true;
    }
    return 0;
}

// Registers the callback handlers to act on property changes
auto registerTimeParamHandlers()
{
    std::string value {};
    keyReader get = std::make_pair("time_mode", &getSystemSettings);
    valueUpdater set = std::make_pair(value, &updateTimeMode);
    gTimeParams.emplace(get, set);

    get = std::make_pair("time_owner", &getSystemSettings);
    set = std::make_pair(value, &updateTimeOwner);
    gTimeParams.emplace(get, set);

    get = std::make_pair("use_dhcp_ntp", &getSystemSettings);
    set = std::make_pair(value, &updateNetworkSettings);
    gTimeParams.emplace(get, set);

    return;
}

int main(int argc, char* argv[])
{
    sd_event_source* eventSource = nullptr;
    sd_event* event = nullptr;

    auto r = -1;

    r = sd_bus_default_system(&gTimeBus);
    if (r < 0)
    {
        std::cerr << "Error" << strerror(-r)
                  <<"connecting to system bus" << std::endl;
        goto finish;
    }

    std::cout <<"Registering dbus methods" << std::endl;
    r = sd_bus_add_object_vtable(gTimeBus,
                                 &gTimeSlot,
                                 timeObjPath,
                                 timeBusName,
                                 timeServicesVtable,
                                 NULL);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<"adding timer services vtable" << std::endl;
        goto finish;
    }

    // create a sd_event object and add handlers
    r = sd_event_default(&event);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<"creating an sd_event" << std::endl;
        goto finish;
    }

    // Handlers called by sd_event when an activity
    // is observed in event loop
    r = registerCallbackHandlers(event, eventSource);
    if (r < 0)
    {
        std::cerr << "Error setting up callback handlers" << std::endl;
        goto finish;
    }

    // Handlers called by sd_event when an activity
    // is observed in event loop
    registerTimeParamHandlers();

    // Reads the properties from settings and applies changes if needed
    r = processInitialSettings();
    if (r < 0)
    {
        std::cerr << "Error processing initial settings" << std::endl;
        goto finish;
    }

    // Claim the bus
    r = sd_bus_request_name(gTimeBus, timeBusName, 0);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "acquiring service name" << std::endl;
        goto finish;
    }

    // Wait for the work
    r = sd_event_loop(event);

finish:
    eventSource = sd_event_source_unref(eventSource);
    event = sd_event_unref(event);

    if (r < 0)
    {
        std::cerr << "Failure: " <<  strerror(-r) << std::endl;
    }

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
