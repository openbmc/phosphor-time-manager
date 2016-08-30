#include <map>
#include <systemd/sd-bus.h>

class TimeConfig
{
public:
    // Possible time modes
    enum class timeModes
    {
        NTP = 0,
        MANUAL,
        INVALID
    };

    // Possible time owners
    enum class timeOwners
    {
        BMC = 0,
        HOST,
        SPLIT,
        BOTH,
        INVALID
    };

    TimeConfig();

private:
    // Bus initialised by manager
    sd_bus *iv_dbus;

    // NTP / MANUAL
    timeModes  iv_CurrTimeMode;
    timeModes  iv_RequestedTimeMode;

    // BMC / HOST / SPLIT / BOTH
    timeOwners iv_CurrTimeOwner;
    timeOwners iv_RequestedTimeOwner;

    // 'yes' / 'no'
    std::string iv_CurrDhcpNtp;

    // Dictated by state of pgood
    bool        iv_SettingChangeAllowed;

    // Needed to nudge Time Manager to reset offset
    bool        iv_ResetHostOffset;

    // Acts on the time property changes / reads initial property
    using READER = std::string (TimeConfig::*) (std::string);
    using UPDATER = int (TimeConfig::*) (std::string);

    // Pair of reader and updater
    using FNCTR = std::pair<READER, UPDATER>;

    // Map of [Key,FNCTR]
    std::map<std::string, FNCTR> gTimeParams;

    const char* timeModeFile = "/var/lib/obmc/saved_timeMode";
    const char* timeOwnerFile = "/var/lib/obmc/saved_timeOwner";
    const char* dhcpNtpFile = "/var/lib/obmc/saved_dhcpNtp";

    // Wrapper that looks up in the mapper service
    // and returns name of the service
    char* getProvider(const char*);

    // Populates map with all the callback routines
    void registerTimeParamHandlers();

    // Reads all the saved data from the last run
    int readPersistentData();

    // Updates the 'ntp' field in systemd/timedate1,
    // which enables / disables NTP syncing
    int modifyNtpSettings(const timeModes newTimeMode);

    // Takes a system setting parameter and returns its value
    // provided the specifier is a string.
    std::string getSystemSettings(const std::string);

    // Reads the data hosted by /org/openbmc/control/power0
    std::string getPowerSetting(const std::string);

    // Receives the Mode string and applies only if conditions allow it.
    int updateTimeMode(std::string newModeStr);

    // Receives the Owner string and applies only if conditions allow it.
    int updateTimeOwner(const std::string newOwnerStr);

    // Updates .network file with UseNtp=
    // provided by Network Manager
    int updateNetworkSettings(const std::string);

    // Gets called whenever there is a change in state of pgood
    int processPgoodChange(std::string);

public:
    inline auto getCurrTimeMode()
    {
        return iv_CurrTimeMode;
    }

    inline auto getCurrTimeOwner()
    {
        return iv_CurrTimeOwner;
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
    // Generic file reader used to read time mode,
    // time owner, host time and host offset
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

    // Generic file writer used to write time mode,
    // time owner, host time and host offset
    template <typename T>
    int writeData(const char* fileName, T data)
    {
        std::ofstream file(fileName, std::ios::out);
        file << data;
        file.close();
        return 0;
    }

    // Reads persistant data and also initial settings
    int processInitialSettings(sd_bus*);

    // Given a time mode string, returns the equivalent enum
    timeModes getTimeMode(std::string timeModeStr);

    // Accepts timeMode enum and returns it's string equivalent
    std::string modeStr(const timeModes timeMode);

    // Given a time owner string, returns the equivalent enum
    timeOwners getTimeOwner(std::string timeOwnerStr);

    // Accepts timeOwner enum and returns it's string equivalent
    std::string ownerStr(const timeOwners timeOwner);

    // Gets called when the settings property changes
    // Walks the map and then applies the changes
    int updatePropertyVal(std::string key, std::string value);
};
