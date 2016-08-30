#include <systemd/sd-bus.h>
#include <fstream>
#include <string>
#include <chrono>
#include "time-config.hpp"

// Fwd declarion to enable passing offset to Host
class TimeManager;

class Time
{
public:
    // Sets time using systemd/timedate1
    int SetTimeOfDay(const std::chrono::microseconds&);

    // Gets BMC time
    std::chrono::microseconds GetBaseTime();

    // Converts microseconds to human readable time value
    std::string ConvertToStr(const std::chrono::microseconds&);

    // Needed to validate
    std::unique_ptr<TimeConfig*> config;

    // Needed to lookup as part of Host Get/Set
    std::unique_ptr<std::chrono::microseconds*> iv_Offset;
};

class BmcTime: public Time
{
private:
    BmcTime();

public:
    int GetTime(sd_bus_message*, sd_bus_error*);

    int SetTime(sd_bus_message*, sd_bus_error*);

    // Needed to consult config while Set/Get Time
    BmcTime(TimeConfig*);
};

class HostTime: public Time
{
private:
    HostTime();

    // Way to communicate the new Host Offset after the SetTime
    std::chrono::microseconds changedOffset;

public:
    int GetTime(sd_bus_message*, sd_bus_error*);

    int SetTime(sd_bus_message*, sd_bus_error*);

    // Needed to consult config while Set/Get Time
    HostTime(TimeConfig*, std::chrono::microseconds*);

    inline std::chrono::microseconds getChangedOffset()
    {
        return changedOffset;
    }
};

class TimeManager
{
private:

    TimeManager();

    // What was the last known host offset.
    std::chrono::microseconds iv_HostOffset;

    // How long was the BMC up for prior to this boot
    std::chrono::microseconds iv_UptimeUsec;

    // Used for registering sd_bus callback handlers.
    sd_event_source* iv_EventSource;
    sd_event* iv_Event;

    // Dbus communication enablers.
    const char* const iv_BusName = "org.openbmc.TimeManager";
    const char* const iv_ObjPath  =  "/org/openbmc/TimeManager";
    const char* const iv_IntfName =  "org.openbmc.TimeManager";

    // Store the offset in File System. Read back when TimeManager starts.
    const char* const iv_HostOffsetFile = "/var/lib/obmc/saved_host_offset";

    sd_bus* iv_TimeBus;
    sd_bus_slot* iv_TimeSlot;

    // Calls all the initial APIs to get things in place.
    int setupTimeManager(void);

    // Singleton instance
    static TimeManager *instance;
public:
    // Maintains *all* the config that is needed for TimeManager.
    TimeConfig config;

    // Wrappers to dbus calls
    int GetTime(sd_bus_message*, void*, sd_bus_error*);
    int SetTime(sd_bus_message*, void*, sd_bus_error*);

    static TimeManager* getInstance()
    {
        if(!instance)
            instance = new TimeManager();
        return instance;
    }

    // Called whenever there is an activity on the Time Manager Dbus.
    static int processSdBusMessage(sd_event_source* es, int fd,
                                   uint32_t revents, void* userdata);

    // Called whenever there is a time change event
    static int processTimeChange(sd_event_source* es, int fd,
                                 uint32_t revents, void* userdata);

    // Called whenever a settings property is changed
    static int processPropertyChange(sd_bus_message*,
                                     void*,sd_bus_error*);

    // Called whenever a pgood is changed
    static int processPgoodChange(sd_bus_message*,
                                  void*,sd_bus_error*);

    // All sd_event callback handlers
    int registerCallbackHandlers();

    // Resets iv_HostOffset
    int resetHostOffset();

    // Reads the previous offset from file system
    int readPersistentData();

    // Executes sd_bus_wait loop
    int waitForClientRequest();

    inline auto getHostOffset()
    {
        return iv_HostOffset;
    }

    inline auto updateHostOffset(const std::chrono::microseconds& newOffset)
    {
        iv_HostOffset = newOffset;
    }

    inline auto getUptimeUsec()
    {
        return iv_UptimeUsec;
    }

    inline auto updateUptimeUsec(const std::chrono::microseconds& newUpTime)
    {
        iv_UptimeUsec = newUpTime;
    }

    inline auto getHostOffsetFile()
    {
        return iv_HostOffsetFile;
    }

    inline sd_bus* getTimeBus()
    {
        return iv_TimeBus;
    }

    inline sd_bus_slot* getTimeSlot()
    {
        return iv_TimeSlot;
    }

    inline auto getBusName()
    {
        return iv_BusName;
    }

    inline auto getObjPath()
    {
        return iv_ObjPath;
    }

    inline auto getIntfName()
    {
        return iv_IntfName;
    }
};
