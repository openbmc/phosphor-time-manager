#include <systemd/sd-bus.h>
#include <fstream>
#include <string>
#include <chrono>
#include "time-config.hpp"

/** @class Time
 *  @brief Base class catering to common data needed by implementations
 */
class Time
{
public:
    /** @brief Takes time in microseconds and uses systemd/timedated
     *         to set the system time
     *
     *  @param[in] timeOfDayUsec - Time value in microseconds
     *  @return                  - Status of time set operation
     */
    int setTimeOfDay(const std::chrono::microseconds& timeOfDayUsec);

    /** @brief Reads BMC time
     *
     *  @param[in]  - None
     *  @return     - time read in microseconds
     */
    std::chrono::microseconds getBaseTime();

    /** @brief Converts microseconds to human readable time value
     *
     *  @param[in] timeInUsec  - Time value in microseconds
     *  @return                - Time converted to human readable format
     */
    std::string convertToStr(const std::chrono::microseconds& timeInUsec);

    /** @brief pointer to config information due to need of policy data */
    const TimeConfig* const config;

    /** Needed to access configuration variables */
    Time(const TimeConfig* const);

private:
    Time();
};

/** @class BmcTime
 *  @brief Provides time Set and time Get operations for BMC target
 */
class BmcTime: public Time
{
public:
    /** @brief Called when the time is to be read on BMC
     *
     *  @param[in] m          - sd_bus message.
     *  @param[out] retError  - Error reporting structure
     *  @return               - On no error, time read which is specified in
     *                          microseonds and also in human readable format.
     *
     *                        - On error, retError populated
     */
    int getTime(sd_bus_message* m, sd_bus_error* retError);

    /** @brief Called when the time is to be set on BMC
     *
     *  @param[in] m          - sd_bus message encapsulating time string
     *  @param[out] retError  - Error reporting structure
     *  @return               - On no error, 0, -1 otherwise or retError thrown
     */
    int setTime(sd_bus_message* m, sd_bus_error* retError);

    /** @brief constructor */
    BmcTime(const TimeConfig* const);

private:
    BmcTime();
};

/** @class HostTime
 *  @brief Provides time Set and time Get operations for BMC target
 */
class HostTime: public Time
{
public:
    /** @brief Called when IPMI_GET_SEL_TIME is called
     *
     *  @param[in] m          - sd_bus message.
     *  @param[out] retError  - Error reporting structure
     *  @return               - On no error, time read which is specified in
     *                          microseonds and also in human readable format.
     *
     *                        - On error, retError populated
     */
    int getTime(sd_bus_message*, sd_bus_error*);

    /** @brief Called when the IPMI_SET_SEL_TIME is called
     *
     *  @param[in] m          - sd_bus message encapsulating time in seconds
     *  @param[out] retError  - Error reporting structure
     *  @return               - On no error, 0, -1 otherwise or retError thrown
     */
    int setTime(sd_bus_message*, sd_bus_error*);

    /** @brief constructor */
    HostTime(const TimeConfig* const, const std::chrono::microseconds* const);

    /** @brief When the owner is SPLIT, the delta between HOST's time and BMC's
     *         time needs to be saved and this  function returns current delta.
     *
     *  @param[in] - None
     *  @return    - offset in microseconds
     */
    inline std::chrono::microseconds getChangedOffset() const
    {
        return changedOffset;
    }

    /** @brief pointer to host's offset in case of SPLIT owner */
    const std::chrono::microseconds* const iv_Offset;

private:
    HostTime();

    /** @brief The delta offset of Host and BMC time */
    std::chrono::microseconds changedOffset;
};

/** @class TimeManager
 *  @brief Caters to client requests with Set and Get time and configuration
 *         changes
 */
class TimeManager
{
public:
    // Do not have a usecase of copying this object so disable
    TimeManager();
    ~TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;
    TimeManager(TimeManager&&) = delete;
    TimeManager& operator=(TimeManager&&) = delete;

    // Maintains *all* the config that is needed for TimeManager.
    TimeConfig config;

    /** @brief Callback handlers invoked by dbus on GetTime client requests
     *
     *  @param[in] m         - sd_bus message encapsulating the time target
     *  @param[in] userdata  - context that is filled while registering this
     *  @param[out] retError - Error reporting mechanism
     *
     *  @return              - On no error, time in microseconds and human
     *                         readable string. retError otherwise.
     */
    int getTime(sd_bus_message* m, void* userdata, sd_bus_error* retError);

    /** @brief Callback handlers invoked by dbus on SetTime client requests
     *
     *  @param[in] m         - sd_bus message encapsulating the time target and
     *                         time value either in string or in seconds.
     *  @param[in] userdata  - client context that is filled while registering
     *  @param[out] retError - Error reporting mechanism
     *
     *  @return              - On no error, time in microseconds and human
     *                         readable string. retError otherwise.
     */
    int setTime(sd_bus_message* m, void* userdata, sd_bus_error* retError);

    /** @brief sd_event callback handlers on the requests coming in dbus
     *         These are actually GetTime and SetTime requests
     *
     * @param[in] es       - Event Structure
     * @param[in] fd       - file descriptor that had 'read' activity
     * @param[in] revents  - generic linux style return event
     * @param[in] userdata - Client context filled while registering
     *
     * @return             - 0 for success, failure otherwise.
     */
    static int processSdBusMessage(sd_event_source* es, int fd,
                                   uint32_t revents, void* userdata);

    /** @brief sd_event callback handler called whenever there is a
     *         time change event indicated by timerfd expiring. This happens
     *         whenever the time is set on BMC by any source.
     *
     * @param[in] es       - Event Structure
     * @param[in] fd       - file descriptor that had 'read' activity
     * @param[in] revents  - generic linux style return event
     * @param[in] userdata - Client context filled while registering
     *
     * @return             - 0 for success, failure otherwise.
     */
    static int processTimeChange(sd_event_source* es, int fd,
                                 uint32_t revents, void* userdata);

    /** @brief sd_event callback handler called whenever a settings
     *         property is changed.
     *         This gets called into whenever "time_mode", "time_owner",
     *         "use_dhcp_ntp" properties are changed
     *
     * @param[in] es       - Event Structure
     * @param[in] fd       - file descriptor that had 'read' activity
     * @param[in] revents  - generic linux style return event
     * @param[in] userdata - Client context filled while registering
     *
     * @return             - 0 for success, failure otherwise.
     */
    static int processPropertyChange(sd_bus_message*,
                                     void*,sd_bus_error*);

    /** @brief sd_event callback handler called whenever Pgood property is
     *         changed
     *
     * @param[in] es       - Event Structure
     * @param[in] fd       - file descriptor that had 'read' activity
     * @param[in] revents  - generic linux style return event
     * @param[in] userdata - Client context filled while registering
     *
     * @return             - 0 for success, failure otherwise.
     */
    static int processPgoodChange(sd_bus_message*,
                                  void*,sd_bus_error*);

    /** @brief registers callsback handlers for sd_event loop
     *
     * @param[in] - None
     * @return    - 0 if everything goes well, -1 otherwise
     */
    int registerCallbackHandlers();

    /** @brief Makes the Delta between Host and BMC time as 'ZERO'. This
     *         essentially only means that time owner was SPLIT before
     *         and now changed to something else.
     *
     *  @param[in]  - None
     *  @return     - 0 if everything goes well, -1 otherwise.
     */
    int resetHostOffset();

    /** @brief Reads what was the last delta offset stored in file
     *
     *  @param[in] - None
     *  @return    - 0 if everything goes well, -1 otherwise.
     */
    int readPersistentData();

    /** @brief waits on sd_events loop for client requests
     *
     *  @param[in]  - None
     *  @return     - 0 if everything goes well, -1 otherwise.
     */
    int waitForClientRequest();

    inline auto getHostOffset() const
    {
        return iv_HostOffset;
    }

    inline auto updateHostOffset(const std::chrono::microseconds& newOffset)
    {
        iv_HostOffset = newOffset;
    }

    inline auto getUptimeUsec() const
    {
        return iv_UptimeUsec;
    }

    inline auto updateUptimeUsec(const std::chrono::microseconds& newUpTime)
    {
        iv_UptimeUsec = newUpTime;
    }

    inline auto getHostOffsetFile() const
    {
        return iv_HostOffsetFile;
    }

    inline sd_bus* getTimeBus() const
    {
        return iv_TimeBus;
    }

    inline auto getBusName() const
    {
        return cv_BusName;
    }

    inline auto getObjPath() const
    {
        return cv_ObjPath;
    }

    inline auto getIntfName() const
    {
        return cv_IntfName;
    }

private:
    // What was the last known host offset.
    std::chrono::microseconds iv_HostOffset;

    // How long was the BMC up for prior to this boot
    std::chrono::microseconds iv_UptimeUsec;

    // Used for registering sd_bus callback handlers.
    sd_event_source* iv_EventSource;
    sd_event* iv_Event;
    sd_bus* iv_TimeBus;

    // Dbus communication enablers.
    static constexpr auto cv_BusName = "org.openbmc.TimeManager";
    static constexpr auto cv_ObjPath  =  "/org/openbmc/TimeManager";
    static constexpr auto cv_IntfName =  "org.openbmc.TimeManager";

    // Store the offset in File System. Read back when TimeManager starts.
    const char* const iv_HostOffsetFile = "/var/lib/obmc/saved_host_offset";

    /** @brief Sets up internal data structures and callback handler at startup
     *
     * @param[in] - None
     * @return    - 0 if everything goes well, -1 otherwise
     */
    int setupTimeManager(void);
};
