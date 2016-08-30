#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/timerfd.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include "time-utils.hpp"

// Neeed to do this since its not exported outside of the kernel.
// Refer : https://gist.github.com/lethean/446cea944b7441228298
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

// Needed to make sure timerfd does not misfire eventhough we set CANCEL_ON_SET
#define TIME_T_MAX (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

// Needed to use freedesktop/timedate1 services
#define USEC_PER_SEC  ((uint64_t) 1000000ULL)

const char* WATCH_SETTING_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

const char* WATCH_PGOOD_CHANGE =
    "type='signal',interface='org.freedesktop.DBus.Properties',"
    "path='/org/openbmc/control/power0',member='PropertiesChanged'";

sd_bus* gTimeBus = NULL;
sd_bus_slot* gTimeSlot = NULL;

// Globals to support multiple modes and owners
timeMode_t  gCurrTimeMode = NTP;
timeMode_t  gRequestedTimeMode;

timeOwner_t gCurrTimeOwner = BMC;
timeOwner_t gRequestedTimeOwner;

// Used when owner is 'SPLIT'
time_t gHostTimeOffset = 0;
time_t gCurrHostTime = 0;
bool gTimeSettingsChangesAllowed = false;

// This needed to be inside a .c file
extern sd_bus_vtable timeServicesVtable[];

// OpenBMC Time Manager dbus framework
const char* timeBusName =  "org.openbmc.TimeManager";
const char* timeObjPath  =  "/org/openbmc/TimeManager";
const char* timeIntfName =  "org.openbmc.TimeManager";

const char* hostTimeFile = "/var/lib/obmc/saved_host_time";
const char* hostOffsetFile = "/var/lib/obmc/saved_host_offset";

const char* timeModeFile = "/var/lib/obmc/saved_timeMode";
const char* timeOwnerFile = "/var/lib/obmc/saved_timeOwner";

// Reads timeofday and returns time string as %Y-%m-%d %H:%M:%S
// and also number of seconcd.
extern "C" int getBmcTime(sd_bus_message* m, void* userdata,
                          sd_bus_error* retError)
{
    time_t currentBmcTime = 0;

    // Enough for '%Y-%m-%d %H:%M:%S'
    char timeStr[28] = {0};

    printf("Request to get BMC time\n");

    time(&currentBmcTime);

    strftime(timeStr, 26, "%Y-%m-%d %H:%M:%S", localtime(&currentBmcTime));

    return sd_bus_reply_method_return(m, "xs", (int64_t)currentBmcTime, &timeStr);
}

// Designated to be called by IPMI_GET_SEL time from host
extern "C" int getHostTime(sd_bus_message* m, void* userdata,
                           sd_bus_error* retError)
{
    time_t currHostTime = 0;
    time_t currBmcTime = 0;

    // Get the time on BMC and adjust the offset
    time(&currBmcTime);
    if (gCurrTimeOwner == SPLIT)
    {
        currHostTime = currBmcTime + gHostTimeOffset;
    }
    else
    {
        currHostTime = currBmcTime;
    }

    printf("get_host_time: host_time:[%ld], bmc_time:[%ld], host_offset:[%ld]\n",
           currHostTime, currBmcTime, gHostTimeOffset);

    return sd_bus_reply_method_return(m, "x", (int64_t)currHostTime);
}

// Helper function to set the time.
int setTimeOfDay(const time_t timeOfDay)
{
    int r = 0;
    int64_t newTimeUsec = 0;
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // These 2 are for  bypassing some policy
    // checking in the timedate1 service
    bool relative = false;
    bool interactive = false;

    // timedate1 service requires that the time be passed as usec
    newTimeUsec = (int64_t)timeOfDay * USEC_PER_SEC;

    r = sd_bus_call_method(gTimeBus,
                           "org.freedesktop.timedate1",
                           "/org/freedesktop/timedate1",
                           "org.freedesktop.timedate1",
                           "SetTime",
                           &busError,
                           NULL,        // timedate1 does not return response
                           "xbb",
                           (int64_t)newTimeUsec,
                           relative,
                           interactive);

    sd_bus_error_free(&busError);
    return r;
}

// Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
// and then sets the BMC time. If the input time string does not conform to the
// format, an error message is returned.
extern "C" int setBmcTime(sd_bus_message* m, void* userdata,
                          sd_bus_error* retError)
{
    struct tm userTm = {0};
    time_t timeOfDay = 0;
    int r = -1;

    const char* userTimeStr = NULL;
    const char* timeVal = NULL;
    const char* returnMsg = NULL;

    printf("Calling BMC_SET_TIME\n");

    printf("setBmcTime: curr_mode:[%d-%s] curr_owner[%d-%s]",
           gCurrTimeMode, modeStr(gCurrTimeMode),
           gCurrTimeOwner, ownerStr(gCurrTimeOwner));

    if (gCurrTimeMode == NTP)
    {
        returnMsg = "Can not set Time. NTP is enabled";
        goto finish;
    }

    if (gCurrTimeOwner == HOST)
    {
        returnMsg = "Can not set time. Time Owner is HOST";
        goto finish;
    }

    r = sd_bus_message_read(m, "s", &userTimeStr);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] reading user time\n", strerror(-r));
        goto finish;
    }

    // Forced to type case to make compiler happy
    timeVal = (char*)strptime(userTimeStr, "%Y-%m-%d %H:%M:%S", &userTm);
    if (*timeVal != '\0' || timeVal == NULL)
    {
        r = -1;
        returnMsg = "Format error. Use '%%Y-%%m-%%d %%H:%%M:%%S'";
        goto finish;
    }

    // Convert the time structure into number of seconds
    timeOfDay = mktime(&userTm);
    if (timeOfDay < 0)
    {
        r = -1;
        fprintf(stderr, "Error converting tm to seconds\n");
        goto finish;
    }

    // Set REALTIME and also update hwclock
    r = setTimeOfDay(timeOfDay);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] setting time on BMC\n", strerror(-r));
    }

finish:
    if (!returnMsg)
    {
        returnMsg = r < 0 ? "Failure setting time" : "Success";
    }

    return sd_bus_reply_method_return(m, "is", r, returnMsg);
}

// Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
// and then sets the BMC time. If the input time string does not conform to the
// format, an error message is returned.
extern "C" int setHostTime(sd_bus_message* m, void* userdata,
                           sd_bus_error* retError)
{
    time_t currBmcTime = 0;
    time_t newHostTime = 0;

    const char* returnMsg = NULL;

    // We should not return failure to host just becase
    // policy does not allow it
    int r = 0;

    printf("setHostTime: curr_mode:[%d-%s] curr_owner:[%d-%s], currHostTime:[%ld],"
           " host_offset:[%ld]\n", gCurrTimeMode, modeStr(gCurrTimeMode),
           gCurrTimeOwner, ownerStr(gCurrTimeOwner),
           gCurrHostTime, gHostTimeOffset);

    if (gCurrTimeOwner == BMC)
    {
        returnMsg = "Can not set time. Current time owner is BMC";
        goto finish;
    }

    r = sd_bus_message_read(m, "x", &newHostTime);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] reading host time\n", strerror(-r));
        goto finish;
    }

    printf("setHostTime: newHostTime:[%ld]\n", newHostTime);

    if (gCurrTimeOwner == SPLIT)
    {
        gCurrHostTime = newHostTime;

        // Adjust the host offset
        time(&currBmcTime);
        gHostTimeOffset = gCurrHostTime - currBmcTime;

        // Persist this new one
        r = writeData(hostTimeFile, &gCurrHostTime,
                      sizeof(gCurrHostTime));
        if (r < 0)
        {
            fprintf(stderr, "Error saving host_time:[%ld]\n",
                    gCurrHostTime);
        }
        // FIXME : Does it make sense to abort ?

        r = writeData(hostOffsetFile, &gHostTimeOffset,
                      sizeof(gHostTimeOffset));
        if (r < 0)
        {
            fprintf(stderr, "Error saving host_offset:[%ld]\n",
                    gHostTimeOffset);
        }

        // We are not doing any time settings in BMC
        printf("setHostTime: Updated: host_time:[%ld], host_offset:[%ld]\n",
               gCurrHostTime, gHostTimeOffset);

        goto finish;
    }

    // We are okay to update time in as long as BMC is not the owner
    r = setTimeOfDay(newHostTime);

finish:
    if (returnMsg == NULL)
    {
        returnMsg = r < 0 ? "Failure setting time" : "Success";
    }

    return sd_bus_reply_method_return(m, "is", r, returnMsg);
}

// Property reader
extern "C" int getTimeManagerProperty(sd_bus* bus, const char* path,
                                 const char* interface, const char* property,
                                 sd_bus_message* m, void* userdata,
                                 sd_bus_error* error)
{
    if (!strcmp(property, "curr_time_mode"))
    {
        return sd_bus_message_append(m, "s", modeStr(gCurrTimeMode));
    }
    else if (!strcmp(property, "curr_time_owner"))
    {
        return sd_bus_message_append(m, "s", ownerStr(gCurrTimeOwner));
    }
    else if (!strcmp(property, "requested_time_mode"))
    {
        return sd_bus_message_append(m, "s", modeStr(gRequestedTimeMode));
    }
    else if (!strcmp(property, "requested_time_owner"))
    {
        return sd_bus_message_append(m, "s", ownerStr(gRequestedTimeOwner));
    }
    return 0;
}

// Gets called into  by sd_event on an activity seen on sd_bus
extern "C" int processSdBusMessage(sd_event_source* es, int fd,
                                   uint32_t revents, void* userdata)
{
    int r = sd_bus_process(gTimeBus, NULL);
    if (r < 0)
    {
        printf("Error [%s] processing sd_bus message:\n",
               strerror(-r));
    }
    return r;
}

// Gets called into by sd_event on any time SET event
int processTimeChange(sd_event_source* es, int fd,
                      uint32_t revents, void* userdata)
{
    char buffer[64] = {0};
    time_t currBmcTime = 0;
    int r = 0;

    // We are not interested in the data here. Need to read time again .
    // So read until there is something here in the FD
    while (read(fd, &buffer, 64) > 0);

    printf("process_time_change: curr_mode:[%d-%s] curr_owner:[%d-%s],"
           " currHostTime:[%ld], host_offset:[%ld]\n",
           gCurrTimeMode, modeStr(gCurrTimeMode),
           gCurrTimeOwner, ownerStr(gCurrTimeOwner),
           gCurrHostTime, gHostTimeOffset);

    // Read the current BMC time and adjust the
    // host time offset if the mode is  SPLIT
    if (gCurrTimeOwner == SPLIT)
    {
        time(&currBmcTime);
        gHostTimeOffset = gCurrHostTime - currBmcTime;

        printf("process_time_change: Updated: currBmcTime:[%ld],"
               " currHostTime:[%ld], host_offset:[%ld]\n",
               currBmcTime, gCurrHostTime, gHostTimeOffset);

        // Persist this
        r = writeData(hostOffsetFile, &gHostTimeOffset,
                      sizeof(gHostTimeOffset));
        if (r < 0)
        {
            fprintf(stderr, "Error saving host_offset:[%ld]\n",
                    gHostTimeOffset);
        }
    }

    return r;
}

// Accepts the time mode and makes necessary changes to timedate1
int modifyNtpSettings(const timeMode_t newTimeMode)
{
    int r = -1;
    int ntpChangeOp = 0;

    // Error return mechanism
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // Pass '1' -or- '0' to SetNTP method indicating Enable/Disable
    ntpChangeOp = (newTimeMode == NTP) ? 1 : 0;

    printf("Applying NTP setting..\n");

    r = sd_bus_call_method(gTimeBus,
                           "org.freedesktop.timedate1",
                           "/org/freedesktop/timedate1",
                           "org.freedesktop.timedate1",
                           "SetNTP",
                           &busError,
                           NULL,    // timedate1 does not return response
                           "bb",
                           ntpChangeOp,       // '1' means Enable
                           0);              // '0' meaning no policy-kit
    if (r < 0)
    {
        printf("Error [%s] changing time Mode\n", strerror(-r));
    }
    else
    {
        printf("SUCCESS. NTP setting is now:[%s]\n",
               (ntpChangeOp) ? "Enabled" : "Disabled");

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
int updtTimeMode(const timeMode_t timeMode)
{
    int r = 0;

    printf("Requested_Mode:[%d-%s], Current_Mode:[%d-%s]\n",
           timeMode, modeStr(timeMode),
           gCurrTimeMode, modeStr(gCurrTimeMode));

    if (timeMode == gCurrTimeMode)
    {
        printf("Mode is already set to :[%d-%s]\n",
               timeMode, modeStr(timeMode));
        return 0;
    }

    // Also, if the time owner is HOST, then we should not allow NTP.
    // However, it may so happen that there are 2 pending requests, one for
    // changing to NTP and other for changing owner to something not HOST.
    // So check if there is a pending timeOwner change and if so, allow NTP
    // if the current is HOST and requested is non HOST.

    if (gCurrTimeOwner == HOST &&
        gRequestedTimeOwner == HOST &&
        timeMode == NTP)
    {
        printf("Can not set mode to NTP with HOST as owner\n");
        return 0;
    }

    if (gTimeSettingsChangesAllowed)
    {
        r = modifyNtpSettings(timeMode);
        if (r < 0)
        {
            fprintf(stderr, "Error changing TimeMode settings");
        }
        else
        {
            gCurrTimeMode = timeMode;
        }

        printf("Current_Mode changed to:[%d-%s]\n", gCurrTimeMode,
               modeStr(gCurrTimeMode));

        // Need this when we either restart or come back from reset
        r = writeData(timeModeFile, &gCurrTimeMode,
                      sizeof(&gCurrTimeMode));
    }
    else
    {
        printf("Deferring update until system state allows it\n");
    }

    return r;
}

// Manipulates time owner if the system setting allows it
int updtTimeOwner(const timeOwner_t timeOwner)
{
    int r = 0;

    // Needed when owner changes to HOST
    timeMode_t forcedModeManual = MANUAL;

    // If owner is HOST, then we need to force mode to MANUAL. keeping it
    // here to cover a case where we start with NTP and HOST config
    if (gCurrTimeOwner == HOST && gRequestedTimeOwner == HOST)
    {
        printf("Forcing the mode to MANUAl\n");
        r = updtTimeMode(forcedModeManual);
        if (r < 0)
        {
            fprintf(stderr, "Error forcing the mode to MANUAL\n");
            return r;
        }
    }

    if (timeOwner == gCurrTimeOwner)
    {
        printf("Owner is already set to :[%d-%s]\n",
               timeOwner, ownerStr(timeOwner));
        return 0;
    }

    printf("Requested_Owner:[%d-%s], Current_Owner:[%d-%s]\n",
           timeOwner, ownerStr(timeOwner),
           gCurrTimeOwner, ownerStr(gCurrTimeOwner));

    if (gTimeSettingsChangesAllowed)
    {
        // If we are transitioning from SPLIT to something else,
        // reset the host offset.
        if (gCurrTimeOwner == SPLIT &&
            gRequestedTimeOwner != SPLIT)
        {
            gHostTimeOffset = 0;
            gCurrHostTime = 0;

            // Persist this new one
            // FIXME : Does it make sense to abort on err
            r = writeData(hostTimeFile, &gCurrHostTime,
                          sizeof(gHostTimeOffset));
            if (r < 0)
            {
                fprintf(stderr, "Error saving host_time:[%ld]\n",
                        gCurrHostTime);
            }

            r = writeData(hostOffsetFile, &gHostTimeOffset,
                          sizeof(gHostTimeOffset));
            if (r < 0)
            {
                fprintf(stderr, "Error saving host_offset:[%ld]\n",
                        gHostTimeOffset);
            }
        }
        gCurrTimeOwner = timeOwner;

        printf("Current_Owner is now:[%d-%s]\n", gCurrTimeOwner,
               ownerStr(gCurrTimeOwner));

        // HOST and NTP are exclusive
        if (gCurrTimeOwner == HOST)
        {
            printf("Forcing the mode to MANUAL\n");
            r = updtTimeMode(forcedModeManual);
            if (r < 0)
            {
                fprintf(stderr, "Error forcing the mode to MANUAL\n");
                return r;
            }
        }

        // Need this when we either restart or come back from reset
        r = writeData(timeOwnerFile, &gCurrTimeOwner,
                      sizeof(gCurrTimeOwner));
    }
    else
    {
        printf("Deferring update until system state allows it\n");
    }

    return r;
}

// Called by sd_event when Properties are changed in Control/power0
// Interested in change to 'pgood'
int processPgoodChange(const int newPgood)
{
    int r = 0;

    // Indicating that we are safe to apply any changes
    if (newPgood == 0)
    {
        gTimeSettingsChangesAllowed = true;
        printf("Changing the time settings allowed now\n");
    }
    else
    {
        gTimeSettingsChangesAllowed = false;
        printf("Changing the time settings is *NOT* allowed now\n");
    }

    // if we have had users that changed the time settings
    // when we were not ready yet, do it now.
    if (gRequestedTimeOwner != gCurrTimeOwner)
    {
        r = updtTimeOwner(gRequestedTimeOwner);
        if (r < 0)
        {
            fprintf(stderr, "Error updating new time owner\n");
            return r;
        }
        printf("New timeOwner is :[%d-%s]\n", gRequestedTimeOwner,
               ownerStr(gRequestedTimeOwner));
    }

    if (gRequestedTimeMode != gCurrTimeMode)
    {
        r = updtTimeMode(gRequestedTimeMode);
        if (r < 0)
        {
            fprintf(stderr, "Error updating new time mode\n");
            return r;
        }
        printf("New time mode is :[%d-%s]\n", gRequestedTimeMode,
               modeStr(gRequestedTimeMode));
    }

    return 0;
}

// Called by sd_event when Properties are changed in settingsd.
// Interested in changes to 'timeMode', 'timeOwner' and 'use_dhcp_ntp'
int processPropertyChange(sd_bus_message* m, void* user_data,
                          sd_bus_error* retError)
{
    const char* key = NULL;
    const char* newTimeMode = NULL;
    const char* newTimeOwner = NULL;
    const char* useDhcpNtp = NULL;

    int newPgood = -1;
    int r = -1;

    printf("Settings CHANGED\n");

    // input data is "sa{sv}as" and we are just interested in a{sv}
    r = sd_bus_message_skip(m, "s");
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] skipping interface name in data\n",
                strerror(-r));
        goto finish;
    }

    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] entering the dictionary\n",
                strerror(-r));
        goto finish;
    }

    while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY,
                "sv")) > 0)
    {
        r = sd_bus_message_read(m, "s", &key);
        if (r < 0)
        {
            fprintf(stderr, "Error [%s] reading the key from dict\n",
                    strerror(-r));
            // Can not continue here since the next
            // enter would result in error anyway
            goto finish;
        }

        if (!strcmp(key, "time_mode"))
        {
            r = sd_bus_message_read(m, "v", "s", &newTimeMode);
            if (r < 0)
            {
                fprintf(stderr, "Error [%s] reading timeMode\n",
                        strerror(-r));
                goto finish;
            }
        }
        else if (!strcmp(key, "time_owner"))
        {
            r = sd_bus_message_read(m, "v", "s", &newTimeOwner);
            if (r < 0)
            {
                fprintf(stderr, "Error [%s] reading timeOwner\n",
                        strerror(-r));
                goto finish;
            }
        }
        else if (!strcmp(key, "use_dhcp_ntp"))
        {
            r = sd_bus_message_read(m, "v", "s", &useDhcpNtp);
            if (r < 0)
            {
                fprintf(stderr, "Error [%s] reading use_dhcp_ntp\n",
                        strerror(-r));
                goto finish;
            }
        }
        else if (!strcmp(key, "pgood"))
        {
            r = sd_bus_message_read(m, "v", "i", &newPgood);
            if (r < 0)
            {
                fprintf(stderr, "Error [%s] reading pgood\n",
                        strerror(-r));
                goto finish;
            }
        }
        else
        {
            sd_bus_message_skip(m, "v");
        }
    }

    // Need to verify if the requested values can be honored now. Else those
    // will be honored only when we change the system state to BMC_READY and
    // HOST_POWERED_OFF
    if (newTimeMode)
    {
        gRequestedTimeMode = getTimeMode(newTimeMode);
        r = updtTimeMode(gRequestedTimeMode);
        if (r < 0)
        {
            fprintf(stderr, "Error updating new time mode\n");
            goto finish;
        }
    }

    if (newTimeOwner)
    {
        gRequestedTimeOwner = getTimeOwner(newTimeOwner);
        r = updtTimeOwner(gRequestedTimeOwner);
        if (r < 0)
        {
            fprintf(stderr, "Error updating new time owner\n");
            goto finish;
        }
    }

    // This will make updates to .network file
    if (useDhcpNtp)
    {
        r = updtNetworkSettings(useDhcpNtp);
        if (r < 0)
        {
            fprintf(stderr, "Error updating network files\n");
            goto finish;
        }
        printf("New use_dhcp_ntp is :[%s]\n", useDhcpNtp);
    }

    // if we have had a new pgood value, then apply any
    // pending policy changes
    if (newPgood != -1)
    {
        r = processPgoodChange(newPgood);
        if (r < 0)
        {
            fprintf(stderr, "Error updating pending policy changes\n");
        }
    }
finish:
    return r;
}

// Reads the values from 'settingsd' and applies:
// 1) Time Mode
// 2) time Owner
// 3) UseNTP setting
int processInitialSettings(void)
{
    char* initialTimeMode = NULL;
    char* initialTimeOwner = NULL;
    char* initialDhcpNtp = NULL;

    int initPgood = -1;
    int r = -1;

    // Before starting anything, need to read the
    // 'timeOwner' and 'timeMode' * from settings.
    initialTimeMode = getSystemSettings("time_mode");
    initialTimeOwner = getSystemSettings("time_owner");
    initialDhcpNtp = getSystemSettings("use_dhcp_ntp");

    printf("Initial-> timeMode:[%s], timeOwner:[%s], use_dhcp_ntp:[%s]\n",
           initialTimeMode, initialTimeOwner, initialDhcpNtp);

    if (initialTimeMode == NULL ||
        initialTimeOwner == NULL ||
        initialDhcpNtp == NULL)
    {
        fprintf(stderr, "Error reading initial settings\n");
        goto finish;
    }

    // Get the pgood value now so that we can set
    // the allowed to change value
    initPgood = getPgoodValue();
    if (initPgood == 0)
    {
        gTimeSettingsChangesAllowed = true;
        printf("Changing the time settings allowed now\n");
    }
    else
    {
        gTimeSettingsChangesAllowed = false;
        printf("Changing the time settings is *NOT* allowed now\n");
    }

    // Get current current host_time
    r = readData(hostTimeFile, &gCurrHostTime,
                 sizeof(gCurrHostTime));
    if (r < 0)
    {
        fprintf(stderr, "Error reading current host time\n");
        goto finish;
    }
    printf("Last known host_time:[%ld]\n", gCurrHostTime);

    // Get current host_offset
    r = readData(hostOffsetFile, &gHostTimeOffset,
                 sizeof(gHostTimeOffset));
    if (r < 0)
    {
        fprintf(stderr, "Error reading current host offset\n");
        goto finish;
    }
    printf("Last known host_offset:[%ld]\n", gHostTimeOffset);

    // If we are coming back from a reset reload, then need to
    // read what was the last successful Mode and Owner.
    r = readData(timeModeFile, &gCurrTimeMode,
                 sizeof(gCurrTimeMode));
    if (r < 0)
    {
        fprintf(stderr, "Error reading current time mode\n");
        goto finish;
    }
    printf("Last known timeMode:[%d-%s]\n", gCurrTimeMode,
           modeStr(gCurrTimeMode));

    r = readData(timeOwnerFile, &gCurrTimeOwner,
                 sizeof(gCurrTimeOwner));
    if (r < 0)
    {
        fprintf(stderr, "Error reading current time owner\n");
        goto finish;
    }
    printf("Last known timeOwner:[%d-%s]\n", gCurrTimeOwner,
           ownerStr(gCurrTimeOwner));

    // Now that we have verified system states,
    // we can go ahead update others
    gRequestedTimeOwner = getTimeOwner(initialTimeOwner);
    r = updtTimeOwner(gRequestedTimeOwner);
    if (r < 0)
    {
        fprintf(stderr, "Error setting time owner\n");
        goto finish;
    }

    gRequestedTimeMode = getTimeMode(initialTimeMode);
    r = updtTimeMode(gRequestedTimeMode);
    if (r < 0)
    {
        fprintf(stderr, "Error setting time mode\n");
        goto finish;
    }

    r = updtNetworkSettings(initialDhcpNtp);
    if (r < 0)
    {
        fprintf(stderr, "Error updating use_dhcp_ntp\n");
    }

finish:
    free(initialTimeMode);
    free(initialTimeOwner);
    free(initialDhcpNtp);
    return r;
}

// Sets up callback handlers for activities on :
// 1) user request on SD_BUS
// 2) Time change
// 3) Settings change
// 4) System state change;
int registerCallbackHandlers(sd_event* event, sd_event_source* eventSource)
{
    int sdBusFd = -1;
    int timeFd = -1;
    int r = -1;

    // Choose the MAX time that is possible to aviod mis fires.
    struct itimerspec maxTime;
    maxTime.it_value.tv_sec = TIME_T_MAX,

    // Extract the descriptor out of sd_bus construct.
    sdBusFd = sd_bus_get_fd(gTimeBus);

    r = sd_event_add_io(event, &eventSource, sdBusFd, EPOLLIN,
                        processSdBusMessage, NULL);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] adding sd_bus_message handler\n",
                strerror(-r));
        return r;
    }

    // Wake me up *only* if someone SETS the time
    timeFd = timerfd_create(CLOCK_REALTIME, 0);
    timerfd_settime(timeFd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &maxTime,
                    NULL);

    r = sd_event_add_io(event, &eventSource, timeFd, EPOLLIN, processTimeChange,
                        NULL);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] adding time_change handler\n",
                strerror(-r));
        return r;
    }

    // Watch for property changes in settingsd
    r = sd_bus_add_match(gTimeBus, &gTimeSlot, WATCH_SETTING_CHANGE,
                         processPropertyChange, NULL);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] adding property change listener\n",
                strerror(-r));
        return r;
    }

    // Watch for state change. Only reliable one to count on is
    // state of [pgood]. value of [1] meaning host is powering on / powered
    // on. [0] means powered off.
    r = sd_bus_add_match(gTimeBus, &gTimeSlot, WATCH_PGOOD_CHANGE,
                         processPropertyChange, NULL);
    if (r < 0)
        fprintf(stderr, "Error [%s] adding pgood change listener\n",
                strerror(-r));
    return r;
}

int main(int argc, char* argv[])
{
    sd_event_source* eventSource = NULL;
    sd_event* event = NULL;

    int r = -1;

    r = sd_bus_default_system(&gTimeBus);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] connecting to system bus\n",
                strerror(-r));
        goto finish;
    }

    printf("Registering dbus methods\n");
    r = sd_bus_add_object_vtable(gTimeBus,
                                 &gTimeSlot,
                                 timeObjPath,
                                 timeBusName,
                                 timeServicesVtable,
                                 NULL);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] adding timer services vtable\n",
                strerror(-r));
        goto finish;
    }

    // create a sd_event object and add handlers
    r = sd_event_default(&event);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] creating an sd_event\n", strerror(-r));
        goto finish;
    }

    // Handlers called by sd_event when an activity
    // is observed in event loop
    r = registerCallbackHandlers(event, eventSource);
    if (r < 0)
    {
        fprintf(stderr, "Error setting up callback handlers\n");
        goto finish;
    }

    // Reads the properties from settings and applies changes if needed
    r = processInitialSettings();
    if (r < 0)
    {
        fprintf(stderr, "Error processing initial settings\n");
        goto finish;
    }

    // Claim the bus
    r = sd_bus_request_name(gTimeBus, timeBusName, 0);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] acquiring service name\n",
                strerror(-r));
        goto finish;
    }

    // Wait for the work
    r = sd_event_loop(event);

finish:
    eventSource = sd_event_source_unref(eventSource);
    event = sd_event_unref(event);

    if (r < 0)
    {
        fprintf(stderr, "Failure: %s\n", strerror(-r));
    }

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
