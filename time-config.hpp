#include <map>
#include <systemd/sd-bus.h>

/** @class TimeConfig
 *  @brief Maintains various time modes and time owners.
 */
class TimeConfig
{
public:
    /** @brief Supported time modes
     *  NTP     Time sourced by Network Time Server
     *  MANUAL  User of the system need to set the time
     */
    enum class timeModes
    {
        NTP,
        MANUAL
    };

    /** @brief Supported time owners
     *  BMC     Time source may be NTP or MANUAL but it has to be set natively
     *          on the BMC. Meaning, host can not set the time. What it also
     *          means is that when BMC gets IPMI_SET_SEL_TIME, then its ignored.
     *          similarly, when BMC gets IPMI_GET_SEL_TIME, then the BMC's time
     *          is returned.
     *
     *  HOST    Its only IPMI_SEL_SEL_TIME that will set the time on BMC.
     *          Meaning, IPMI_GET_SEL_TIME and request to get BMC time will
     *          result in same value.
     *
     *  SPLIT   Both BMC and HOST will maintain their individual clocks but then
     *          the time information is stored in BMC. BMC can have either NTP
     *          or MANUAL as it's source of time and will set the time directly
     *          on the BMC. When IPMI_SET_SEL_TIME is received, then the delta
     *          between that and BMC's time is calculated and is stored.
     *          When BMC reads the time, the current time is returned.
     *          When IPMI_GET_SEL_TIME is received, BMC's time is retrieved and
     *          then the delta offset is factored in prior to returning.
     *
     *  BOTH:   BMC's time is set with whoever that sets the time. Similarly,
     *          BMC's time is returned to whoever that asks the time.
     */
    enum class timeOwners
    {
        BMC,
        HOST,
        SPLIT,
        BOTH
    };

    // Do not have a usecase of copying this object so disable
    TimeConfig();
    ~TimeConfig() = default;
    TimeConfig(const TimeConfig&) = delete;
    TimeConfig& operator=(const TimeConfig&) = delete;
    TimeConfig(TimeConfig&&) = delete;
    TimeConfig& operator=(TimeConfig&&) = delete;

    inline auto getCurrTimeMode()
    {
        return iv_CurrTimeMode;
    }

    inline auto getCurrTimeOwner()
    {
        return iv_CurrTimeOwner;
    }

    inline auto getRequestedTimeMode()
    {
        return iv_RequestedTimeMode;
    }

    inline auto getRequestedTimeOwner()
    {
        return iv_RequestedTimeOwner;
    }

    inline auto resetHostOffset()
    {
        return iv_ResetHostOffset;
    }

    inline void updateResetHostFlag(const bool& value)
    {
        iv_ResetHostOffset = value;
    }

    inline sd_bus* getDbus()
    {
        return iv_dbus;
    }

    /** brief Generic file reader used to read time mode,
     *  time owner, host time,  host offset and useDhcpNtp
     *
     *  @param[in] filename - Name of file where data is preserved.
     *  @return             - File content
     */
    template <typename T>
    T readData(const char* fileName)
    {
        T data = T();
        if(std::ifstream(fileName))
        {
            std::ifstream file(fileName, std::ios::in);
            file >> data;
            file.close();
        }
        return data;
    }

    /** @brief Generic file writer used to write time mode,
     *  time owner, host time,  host offset and useDhcpNtp
     *
     *  @param[in] filename - Name of file where data is preserved.
     *  @param[in] data     - Data to be written to file
     *  @return             - 0 for now. But will be changed to raising
     *                      - Exception
     */
    template <typename T>
    auto writeData(const char* fileName, T&& data)
    {
        std::ofstream file(fileName, std::ios::out);
        file << data;
        file.close();
        return 0;
    }

    /** @brief Reads saved data and populates below properties
     *  - Current Time Mode
     *  - Current Time Owner
     *  - Whether to use NTP settings given by DHCP
     *  - Last known host offset
     *
     *  @param[in] dbus -  Handler to sd_bus used by time manager
     *
     *  @return         -  < 0 for failure and others for success
     */
    int processInitialSettings(sd_bus* dbus);

    /** @brief Accepts time mode string, returns the equivalent enum
     *
     *  @param[in] timeModeStr - Current Time Mode in string
     *
     *  @return                - Equivalent ENUM
     *                         - Input : "NTP", Output: timeModes::NTP
     */
    timeModes getTimeMode(const char* timeModeStr);

    /** @brief Accepts timeMode enum and returns it's string equivalent
     *
     *  @param[in] timeMode - Current Time Mode Enum
     *
     *  @return             - Equivalent string
     *                      - Input : timeModes::NTP, Output : "NTP"
     */
    const char* modeStr(const timeModes& timeMode);

    /** @brief Accepts timeOwner string and returns it's equivalent enum
     *
     *  @param[in] timeOwnerStr - Current Time Owner in string
     *
     *  @return              - Equivalent ENUM
     *                       - Input : "BMC", output : timeOwners::BMC
     */
    timeOwners getTimeOwner(const char* timeOwnerStr);

    /** @brief Accepts timeOwner enum and returns it's string equivalent
     *
     *  @param[in] timeOwner - Current Time Mode Enum
     *
     *  @return              - Equivalent string
     *                       - Input : timeOwners::BMC, Output : "BMC"
     */
    const char* ownerStr(const timeOwners& timeOwner);

    /** @brief Gets called when the settings property changes.
     *  Walks the map and then applies the changes
     *
     *  @param[in] key   - Name of the property
     *  @param[in] value - Value
     *
     *  @return              - < 0 on failure, success otherwise
     */
    int updatePropertyVal(const char* key, const std::string& value);

    // Acts on the time property changes / reads initial property
    using READER = std::string (TimeConfig::*) (const char*);
    using UPDATER = int (TimeConfig::*) (const std::string&);
    using FUNCTOR = std::tuple<READER, UPDATER>;

    // Most of this is statically constructed and PGOOD is added later.
    static std::map<const char*, FUNCTOR> iv_TimeParams;

private:
    // Bus initialised by manager on a call to process initial settings
    sd_bus *iv_dbus;

    /** @brief 'Requested' is what is asked by user in settings
     * 'Current' is what the TimeManager is really using since its not
     *  possible to apply the mode and owner as and when the user updates
     *  Settings. They are only applied when the system power is off.
     */
    timeModes  iv_CurrTimeMode;
    timeModes  iv_RequestedTimeMode;

    timeOwners iv_CurrTimeOwner;
    timeOwners iv_RequestedTimeOwner;

    /** @brief One of the entry in .network file indicates whether the
     *  systemd-timesyncd should use the NTP server list that are sent by DHCP
     *  server or not. If the value is 'yes', then NTP server list sent by DHCP
     *  are used. else entries that are configured statically with NTP= are used
     */
    std::string iv_CurrDhcpNtp;

    /** @brief Dictated by state of pgood. When the pgood value is 'zero', then
     *  its an indication that we are okay to apply any pending Mode / Owner
     *  values. Meaning, Current will be updated with what is in Requested.
     */
    bool        iv_SettingChangeAllowed;

    // Needed to nudge Time Manager to reset offset
    bool        iv_ResetHostOffset;

    const char* const iv_TimeModeFile = "/var/lib/obmc/saved_timeMode";
    const char* const iv_TimeOwnerFile = "/var/lib/obmc/saved_timeOwner";
    const char* const iv_DhcpNtpFile = "/var/lib/obmc/saved_dhcpNtp";

    /** @brief Wrapper that looks up in the mapper service for a given
     *  object path and returns the service name
     *
     *  @param[in] objpath - dbus object path
     *
     *  @return            - unique_ptr holding service name.
     *                     - Caller needs to validate if its populated
     */
    std::unique_ptr<char*> getProvider(const char* objPath);

    /** @brief Helper function for processInitialSettings.
     *  Reads saved data and populates below properties. Only the values are
     *  read from the file system. but it will not take the action on those.
     *  Actions on these are taken by processInitialSettings
     *
     *  - Current Time Mode
     *  - Current Time Owner
     *  - Whether to use NTP settings given by DHCP
     *  - Last known host offset
     *
     *  @return         -  < 0 for failure and success on others
     */
    int readPersistentData();

    /** @brief Updates the 'ntp' field in systemd/timedate1,
     *  which enables / disables NTP syncing
     *
     *  @param[in] newTimeMode - Time Mode Enum
     *
     *  @return                - < 0 on failure and success on others
     */
    int modifyNtpSettings(const timeModes& newTimeMode);

    /** @brief Accepts system setting parameter and returns its value
     *
     *  @param[in] key - Name of the property
     *
     *  @return        - Value as string
     */
    std::string getSystemSettings(const char* key);

    /** @brief Reads the data hosted by /org/openbmc/control/power0
     *
     *  @param[in] key - Name of the property
     *
     *  @return        - Value as string
     */
    std::string getPowerSetting(const char* key);

    /** @brief Accepts Mode string and applies only if conditions allow it.
     *
     *  @param[in] newModeStr - Requested Time Mode
     *
     *  @return               - < 0 on failure, success otherwise
     */
    int updateTimeMode(const std::string& newModeStr);

    /** @brief Accepts Ownere string and applies only if conditions allow it.
     *
     *  @param[in] newOwnerStr - Requested Time Owner
     *
     *  @return               - < 0 on failure, success otherwise
     */

    int updateTimeOwner(const std::string& newownerStr);

    /** @brief Updates .network file with UseNtp= provided by NetworkManager
     *
     *  @param[in] useDhcpNtp - will be 'yes' or 'no'
     *
     *  @return               - < 0 on failure, success otherwise
     */
    int updateNetworkSettings(const std::string& useDhcpNtp);

    /** @brief Accepts current pgood value and then updates any pending mode
     *  or owner requests
     *
     *  @param[in] useDhcpNtp - will be 'yes' or 'no'
     *
     *  @return               - < 0 on failure, success otherwise
     */
    int processPgoodChange(const std::string& newPgood);
};
