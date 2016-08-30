#define _XOPEN_SOURCE
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <array>
#include <unistd.h>
#include <assert.h>
#include <sys/timerfd.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include "time-register.hpp"
#include "time-manager.hpp"

// Neeed to do this since its not exported outside of the kernel.
// Refer : https://gist.github.com/lethean/446cea944b7441228298
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

// Needed to make sure timerfd does not misfire eventhough we set CANCEL_ON_SET
#define TIME_T_MAX (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

// Singleton instance
TimeManager *TimeManager::instance = nullptr;

// Used in time-register.c
extern sd_bus_vtable timeServicesVtable[];

BmcTime::BmcTime(TimeConfig& timeConfig)
{
    config = &timeConfig;
}

HostTime::HostTime(TimeConfig& timeConfig,
                   const std::chrono::microseconds& hostOffset)
{
    config = &timeConfig;
    iv_Offset = &hostOffset;
}

TimeManager::TimeManager()
{
    iv_HostOffset = std::chrono::microseconds(0);
    iv_UptimeUsec = std::chrono::microseconds(0);

    iv_BusName = "org.openbmc.TimeManager";
    iv_ObjPath  =  "/org/openbmc/TimeManager";
    iv_IntfName =  "org.openbmc.TimeManager";
    iv_TimeSlot = nullptr;
    iv_EventSource = nullptr;
    iv_Event = nullptr;
    iv_HostOffsetFile = "/var/lib/obmc/saved_host_offset";

    assert(setupTimeManager() >= 0);
}

// Needed to be standalone extern "C" to register
// as a callback routine with sd_bus_vtable
int GetTime(sd_bus_message* m, void* userdata,
            sd_bus_error* retError)
{
    auto tmgr = TimeManager::getInstance();
    return tmgr->GetTime(m, userdata, retError);
}

int SetTime(sd_bus_message* m, void* userdata,
            sd_bus_error* retError)
{
    auto tmgr = TimeManager::getInstance();
    return tmgr->SetTime(m, userdata, retError);
}

// Property reader
int getTimeManagerProperty(sd_bus* bus, const char* path,
                           const char* interface, const char* property,
                           sd_bus_message* m, void* userdata,
                           sd_bus_error* error)
{
    TimeManager *tmgr = TimeManager::getInstance();

    if (!strcmp(property, "curr_time_mode"))
    {
        return sd_bus_message_append(m, "s",
                                     tmgr->config.modeStr(tmgr->config.getCurrTimeMode()).c_str());
    }
    else if (!strcmp(property, "curr_time_owner"))
    {
        return sd_bus_message_append(m, "s",
                                     tmgr->config.ownerStr(tmgr->config.getCurrTimeOwner()).c_str());
    }
    else if (!strcmp(property, "requested_time_mode"))
    {
        return sd_bus_message_append(m, "s",
                                     tmgr->config.modeStr(tmgr->config.getCurrTimeMode()).c_str());
    }
    else if (!strcmp(property, "requested_time_owner"))
    {
        return sd_bus_message_append(m, "s",
                                     tmgr->config.ownerStr(tmgr->config.getCurrTimeOwner()).c_str());
    }
    return -EINVAL;
}

int TimeManager::GetTime(sd_bus_message* m, void* userdata,
                         sd_bus_error* retError)
{
    const char* target = nullptr;

    // Extract the target and call respective GetTime
    auto r = sd_bus_message_read(m, "s", &target);
    if (r < 0)
    {
        std::cerr << "Error:" << strerror(-r)
                  <<" reading user time" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_IO_ERROR, "Error reading input");

        return sd_bus_reply_method_error(m, retError);
    }

    if (!strcasecmp(target, "bmc"))
    {
        auto time = std::make_unique<BmcTime>(BmcTime(config));
        return time->GetTime(m, retError);
    }
    else if (!strcasecmp(target, "host"))
    {
        auto time = std::make_unique<HostTime>
                    (HostTime(config, iv_HostOffset));
        return time->GetTime(m, retError);
    }
    else
    {
        std::cerr << "Error:" << strerror(-r)
                  <<" Invalid Target" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_IO_ERROR, "Valid targets are BMC or HOST");

        return sd_bus_reply_method_error(m, retError);
    }
}

int TimeManager::SetTime(sd_bus_message* m, void* userdata,
                         sd_bus_error* retError)
{
    long long int timeInUsec {};

    const char* target = nullptr;
    auto r = sd_bus_message_read(m, "s", &target);
    if (r < 0)
    {
        std::cerr << "Error:" << strerror(-r)
                  <<" reading user time" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_IO_ERROR, "Error reading input");

        return sd_bus_reply_method_error(m, retError);
    }

    if (!strcasecmp(target, "bmc"))
    {
        auto time = std::make_unique<BmcTime>(BmcTime(config));
        auto r = time->SetTime(m, retError);
        if (r < 0)
        {
            // This would have the error populated
            return sd_bus_reply_method_error(m, retError);
        }
    }
    else if (!strcasecmp(target, "host"))
    {
        auto time = std::make_unique<HostTime>
                    (HostTime(config, iv_HostOffset));

        auto r = time->SetTime(m, retError);
        if (r < 0)
        {
            // This would have the error populated
            return sd_bus_reply_method_error(m, retError);
        }

        if (config.getCurrTimeOwner() == config.timeOwners::SPLIT)
        {
            auto hostTime = dynamic_cast<HostTime*>(time.get());

            iv_HostOffset = hostTime->getChangedOffset();
            r = config.writeData<long long int>(iv_HostOffsetFile,
                                                iv_HostOffset.count());
            if (r < 0)
            {
                // probably does not make sense to crash on these..
                // The next NTP sync will set things right.
                std::cerr << "Error saving host_offset: "
                          << iv_HostOffset.count() << std::endl;
            }
        }
    }
    return sd_bus_reply_method_return(m, "i", 0);
}

int Time::SetTimeOfDay(const std::chrono::microseconds timeOfDayUsec)
{
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // These 2 are for bypassing some policy
    // checking in the timedate1 service
    auto relative = false;
    auto interactive = false;

    auto r = sd_bus_call_method(config->getDbus(),
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

// Common routine for BMC and HOST Get Time operations
std::chrono::microseconds Time::GetBaseTime()
{
    auto currBmcTime = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>
           (currBmcTime.time_since_epoch());
}

// Accepts the time in microseconds and converts to Human readable format.
std::string Time::ConvertToStr(const std::chrono::microseconds& timeInUsec)
{
    using namespace std::chrono;

    // Convert this to number of seconds;
    auto timeInSec = duration_cast<seconds>(microseconds(timeInUsec));
    auto time_T = static_cast<std::time_t>(timeInSec.count());

    std::ostringstream timeFormat {};
    timeFormat << std::put_time(std::gmtime(&time_T), "%c %Z");

    std::cout << timeFormat.str().c_str() << std::endl;
    return timeFormat.str();
}

// Reads timeofday and returns time string
// and also number of microseconds.
// Ex : Tue Aug 16 22:49:43 2016
int BmcTime::GetTime(sd_bus_message *m, sd_bus_error *retError)
{
    std::cout << "Request to get BMC time: ";

    // Get BMC time
    auto timeInUsec = GetBaseTime();
    auto timeStr = ConvertToStr(timeInUsec);
    return sd_bus_reply_method_return(m, "sx",
                                      timeStr.c_str(), (uint64_t)timeInUsec.count());
}

// Designated to be called by IPMI_GET_SEL time from host
int HostTime::GetTime(sd_bus_message *m, sd_bus_error *retError)
{
    using namespace std::chrono;

    std::cout << "Request to get HOST time" << std::endl;

    // Get BMC time and add Host's offset
    // Referencing the iv_hostOffset of TimeManager object
    auto timeInUsec = GetBaseTime();
    auto hostTime = timeInUsec.count() + iv_Offset->count();

    auto timeStr = ConvertToStr(duration_cast<microseconds>
                                (microseconds(hostTime)));

    std::cout << " Host_time_str: [ " << timeStr
              << " ] host_time usec: [ " << hostTime
              << " ] host_offset: [ " << iv_Offset->count()
              << " ] " << std::endl;

    return sd_bus_reply_method_return(m, "sx", timeStr.c_str(),
                                      (uint64_t)hostTime);
}

// Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
// and then sets the BMC time. If the input time string does not conform to the
// format, an error message is returned.
int BmcTime::SetTime(sd_bus_message *m, sd_bus_error *retError)
{
    tm userTm {};
    const char* userTimeStr = nullptr;

    std::cout << "Request to set BMC time" << std::endl;

    std::cout << "Curr_Mode: " << config->modeStr(config->getCurrTimeMode())
              << " Curr_Owner: " << config->ownerStr(config->getCurrTimeOwner())
              << std::endl;

    if (config->getCurrTimeMode() == config->timeModes::NTP)
    {
        std::cerr << "Can not set time. Mode is NTP" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Current Mode is NTP");

        return -1;
    }

    if(config->getCurrTimeOwner() == config->timeOwners::HOST)
    {
        std::cerr << "Can not set time. Owner is HOST" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Current owner is HOST");

        return -1;
    }

    auto r = sd_bus_message_read(m, "s", &userTimeStr);
    if (r < 0)
    {
        std::cerr << "Error:" << strerror(-r)
                  <<" reading user time" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_IO_ERROR, "Error reading input");
        return r;
    }

    std::cout <<" BMC TIME : " << userTimeStr << std::endl;

    // Convert the time string into tm structure
    std::istringstream timeString {};
    timeString.str(userTimeStr);
    timeString >> std::get_time(&userTm, "%Y-%m-%d %H:%M:%S");
    if (timeString.fail())
    {
        std::cerr << "Error: Incorrect time format" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_INVALID_ARGS, "Incorrect time format");
        return -1;
    }

    // Convert the time structure into number of
    // seconds maintained in GMT. Followed the same that is in
    // systemd/timedate1
    auto timeOfDay = timegm(&userTm);
    if (timeOfDay < 0)
    {
        std::cerr <<"Error converting tm to seconds" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Error converting tm to seconds");
        return -1;
    }

    // Set REALTIME and also update hwclock
    auto timeInUsec = std::chrono::microseconds(
                          std::chrono::seconds(timeOfDay));
    r = SetTimeOfDay(timeInUsec);
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  << "setting time on BMC" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Error setting time on BMC");
    }
    return r < 0 ? r : 0;
}

// Gets the time string from IPMI ( which is currently in seconds since epoch )
// and then sets the BMC time / adjusts the offset depending on current owner
// policy.
int HostTime::SetTime(sd_bus_message *m, sd_bus_error *retError)
{
    using namespace std::chrono;

    std::cout << "Request to SET Host time" << std::endl;

    std::cout << "Curr_Mode: " << config->modeStr(config->getCurrTimeMode())
              << "Curr_Owner: " << config->ownerStr(config->getCurrTimeOwner())
              << "host_offset: " << iv_Offset->count() << std::endl;

    if (config->getCurrTimeOwner() == config->timeOwners::BMC)
    {
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Current Owner is BMC");
        return -1;
    }

    const char* newHostTime = nullptr;
    auto r = sd_bus_message_read(m, "s", &newHostTime);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "reading host time" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_IO_ERROR, "Error reading input");
        return -1;
    }

    // We need to convert the string input to decimal.
    auto hostTimeSec = std::stol(std::string(newHostTime));

    // And then to microseconds
    auto hostTimeUsec = duration_cast<microseconds>(seconds(hostTimeSec));
    std::cout << "setHostTime: HostTimeInUSec: "
              << hostTimeUsec.count() << std::endl;

    if (config->getCurrTimeOwner() == config->timeOwners::SPLIT)
    {
        // Adjust the host offset
        auto bmcTimeUsec = duration_cast<microseconds>
                           (system_clock::now().time_since_epoch());

        // We are not doing any time settings in BMC
        std::cout << "Updated: host_time: [  " << hostTimeUsec.count()
                  << " ] host_offset: [ " << iv_Offset->count()
                  << " ] " << std::endl;

        // Communicate the offset back to manager to update needed.
        changedOffset = hostTimeUsec - bmcTimeUsec;

        return 0;
    }

    // We are okay to update time in as long as BMC is not the owner
    r = SetTimeOfDay(hostTimeUsec);
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  << "setting HOST time" << std::endl;
        *retError = SD_BUS_ERROR_MAKE_CONST(
                        SD_BUS_ERROR_FAILED, "Error setting time");
    }

    return r < 0 ? r : 0;
}

// Gets called into by sd_event on an activity seen on sd_bus
int TimeManager::processSdBusMessage(sd_event_source* es, int fd,
                                     uint32_t revents, void* userdata)
{
    auto tmgr = TimeManager::getInstance();
    return tmgr->processSdBusMsg();
}

int TimeManager::processSdBusMsg()
{
    auto r = sd_bus_process(getTimeBus(), nullptr);
    if (r < 0)
    {
        std::cerr <<"Error: " << strerror(-r)
                  <<" processing sd_bus message:" << std::endl;
    }
    return r;
}

// Gets called into by sd_event on any time SET event
int TimeManager::processTimeChange(sd_event_source* es, int fd,
                                   uint32_t revents, void* userdata)
{
    std::cout << "BMC time changed" << std::endl;

    auto tmgr = TimeManager::getInstance();

    std::array<char, 64> time {};

    // We are not interested in the data here. Need to read time again .
    // So read until there is something here in the FD
    while (read(fd, time.data(), time.max_size()) > 0);

    // Now call the instance method that handles this.
    return tmgr->processTimeChnge();
}

int TimeManager::processTimeChnge()
{
    using namespace std::chrono;

    std::cout <<" Curr_Mode: " << config.modeStr(config.getCurrTimeMode())
              << " Curr_Owner : " << config.ownerStr(config.getCurrTimeOwner())
              << " Host_Offset: " << iv_HostOffset.count() << std::endl;

    // Read the current BMC time and adjust the
    // host time offset if the mode is  SPLIT
    if (config.getCurrTimeOwner() == config.timeOwners::SPLIT)
    {
        // Delta between REAL and Monotonic.
        auto uptimeUsec = duration_cast<microseconds>
                          (system_clock::now().time_since_epoch() -
                           steady_clock::now().time_since_epoch());

        auto deltaTimeUsec = uptimeUsec - iv_UptimeUsec;
#ifdef TIME_MGR_DEBUG
        std::cout << " UPTIME_USEC : " << uptimeUsec.count();
        std::cout << " G_UPTIME_USEC : " << iv_UptimeUsec.count();
        std::cout << " DELTA_TIME_USEC: " << deltaTimeUsec.count();
        std::cout << " PREV_HOST_OFFSET: " << iv_HostOffset.count()
                  << std::endl;
#endif
        // If the BMC time goes backwards, then - of - will handle that.
        iv_HostOffset = iv_HostOffset - deltaTimeUsec;

        std::cout << " UPDATED HOST_OFFSET: "
                  << iv_HostOffset.count() << std::endl;
        iv_UptimeUsec = uptimeUsec;

        // Persist this
        auto r = config.writeData<long long int>
                 (iv_HostOffsetFile, iv_HostOffset.count());
        if (r < 0)
        {
            std::cerr << "Error saving host_offset: "
                      << iv_HostOffset.count() << std::endl;
            return r;
        }
        std::cout << " Updated: Host_Offset: "
                  << iv_HostOffset.count() << std::endl;
    }
    return 0;
}

// Called by sd_event when Properties are changed in settingsd.
// Interested in changes to 'timeMode', 'timeOwner' and 'use_dhcp_ntp'
int TimeManager::processPropertyChange(sd_bus_message* m, void* user_data,
                                       sd_bus_error* retError)
{
    auto tmgr = TimeManager::getInstance();
    return tmgr->processProprtyChange(m, user_data, retError);
}

int TimeManager::processProprtyChange(sd_bus_message* m, void* user_data,
                                      sd_bus_error* retError)
{
    const char* key = nullptr;
    const char* value = nullptr;

    auto newPgood = -1;
    auto r = 0;

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
            r = config.updatePropertyVal(key, value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "processing timeMode" << std::endl;
                goto finish;
            }
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
            r = config.updatePropertyVal(key, value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "processing time_owner" << std::endl;
                goto finish;
            }
            else if (config.resetHostOffset())
            {
                // Must have been a change away from mode SPLIT
                iv_HostOffset = std::chrono::microseconds(0);
                r = config.writeData<long long int>(iv_HostOffsetFile,
                                                    iv_HostOffset.count());
                config.updateResetHostFlag(false);
            }
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
            r = config.updatePropertyVal(key, value);
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "processing dhcp_ntp" << std::endl;
                goto finish;
            }
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
            r = config.updatePropertyVal(key, std::to_string(newPgood));
            if (r < 0)
            {
                std::cerr << "Error: " << strerror(-r)
                          << "processing pgood" << std::endl;
                goto finish;
            }
        }
        else
        {
            sd_bus_message_skip(m, "v");
        }
    }
finish:
    return r;
}

// Sets up callback handlers for activities on :
// 1) user request on SD_BUS
// 2) Time change
// 3) Settings change
// 4) System state change;
int TimeManager::registerCallbackHandlers()
{
    constexpr auto WATCH_SETTING_CHANGE =
        "type='signal',interface='org.freedesktop.DBus.Properties',"
        "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

    constexpr auto WATCH_PGOOD_CHANGE =
        "type='signal',interface='org.freedesktop.DBus.Properties',"
        "path='/org/openbmc/control/power0',member='PropertiesChanged'";

    // Extract the descriptor out of sd_bus construct.
    auto sdBusFd = sd_bus_get_fd(getTimeBus());

    auto r = sd_event_add_io(iv_Event, &iv_EventSource, sdBusFd, EPOLLIN,
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

    auto timeFd = timerfd_create(CLOCK_REALTIME, 0);
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
    r = sd_event_add_io(iv_Event, &iv_EventSource, timeFd, EPOLLIN,
                        processTimeChange, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "adding time_change handler" << std::endl;
        return r;
    }

    // Watch for property changes in settingsd
    r = sd_bus_add_match(iv_TimeBus, &iv_TimeSlot, WATCH_SETTING_CHANGE,
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
    r = sd_bus_add_match(iv_TimeBus, &iv_TimeSlot, WATCH_PGOOD_CHANGE,
                         processPropertyChange, nullptr);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << " adding pgood change listener" << std::endl;
    }
    return r;
}

int TimeManager::setupTimeManager()
{
    auto r = sd_bus_default_system(&iv_TimeBus);
    if (r < 0)
    {
        std::cerr << "Error" << strerror(-r)
                  <<"connecting to system bus" << std::endl;
        goto finish;
    }

    std::cout <<"Registering dbus methods" << std::endl;
    r = sd_bus_add_object_vtable(iv_TimeBus,
                                 &iv_TimeSlot,
                                 getObjPath(),
                                 getBusName(),
                                 timeServicesVtable,
                                 NULL);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<"adding timer services vtable" << std::endl;
        goto finish;
    }

    // create a sd_event object and add handlers
    r = sd_event_default(&iv_Event);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  <<"creating an sd_event" << std::endl;
        goto finish;
    }

    // Handlers called by sd_event when an activity
    // is observed in event loop
    r = registerCallbackHandlers();
    if (r < 0)
    {
        std::cerr << "Error setting up callback handlers" << std::endl;
        goto finish;
    }

    // Need to do this here since TimeConfig may update the necessary owners
    r = config.processInitialSettings(iv_TimeBus);
    if (r < 0)
    {
        std::cerr << "Error setting up configuration params" << std::endl;
        goto finish;
    }

    // Read saved values from previous run
    r = readPersistentData();
    if (r < 0)
    {
        std::cerr << "Error reading persistent data" << std::endl;
        goto finish;
    }

    // Claim the bus
    r = sd_bus_request_name(iv_TimeBus, getBusName(), 0);
    if (r < 0)
    {
        std::cerr << "Error: " << strerror(-r)
                  << "acquiring service name" << std::endl;
        goto finish;
    }
finish:
    if (r < 0)
    {
        iv_EventSource = sd_event_source_unref(iv_EventSource);
        iv_Event = sd_event_unref(iv_Event);
    }
    return r;
}

int TimeManager::readPersistentData()
{
    using namespace std::chrono;

    // Get current host_offset
    // When we reach here, TimeConfig would have been populated and would have
    // applied the owner to SPLIT *if* the system allowed it. So check if we
    // moved away from SPLIT and if so, make offset:0
    if (config.resetHostOffset())
    {
        iv_HostOffset = microseconds(0);
        auto r = config.writeData<long long int>(iv_HostOffsetFile,
                 iv_HostOffset.count());
        if (r < 0)
        {
            std::cerr <<" Error saving offset to file" << std::endl;
            return r;
        }
    }
    else
    {
        auto hostTimeOffset = config.readData<long long int>(iv_HostOffsetFile);
        iv_HostOffset = microseconds(hostTimeOffset);
        std::cout <<"Last known host_offset:" << hostTimeOffset << std::endl;
    }

    //How long was the FSP up prior to 'this' start
    iv_UptimeUsec = duration_cast<microseconds>
                    (system_clock::now().time_since_epoch() -
                     steady_clock::now().time_since_epoch());
    if (iv_UptimeUsec.count() < 0)
    {
        std::cerr <<"Error reading uptime" << std::endl;
        return -1;
    }
    std::cout <<"Initial Uptime Usec: "
              << iv_UptimeUsec.count() << std::endl;
    return 0;
}

// Forever loop
int TimeManager::waitForClientRequest()
{
    return sd_event_loop(iv_Event);
}

int main(int argc, char* argv[])
{
    TimeManager *tmgr = TimeManager::getInstance();

    // Wait for the work
    auto r = tmgr->waitForClientRequest();

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
