#ifndef TIME_UTILS_HPP
#define TIME_UTILS_HPP
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <map>

constexpr auto hostOffsetFile = "/var/lib/obmc/saved_host_offset";

constexpr auto timeModeFile = "/var/lib/obmc/saved_timeMode";
constexpr auto timeOwnerFile = "/var/lib/obmc/saved_timeOwner";
constexpr auto dhcpNtpFile = "/var/lib/obmc/saved_dhcpNtp";

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

// Acts on the time property changes / reads initial property
using READER = std::string (*) (std::string);
using UPDATER = int (*) (std::string);

// Pair of property and reader
using keyReader = std::pair<std::string, READER>;

// Pair of property value and validator
using valueUpdater = std::pair<std::string, UPDATER>;

// Map of [|Key, Reader|value, updater|]
using timeParamsMap = std::map<keyReader, valueUpdater>;

// Given a time mode string, returns the equivalent enum
timeModes getTimeMode(std::string timeMode);

// Accepts timeMode enum and returns it's string equivalent
std::string modeStr(const timeModes timeMode);

// Given a time owner string, returns the equivalent enum
timeOwners getTimeOwner(std::string timeOwner);

// Accepts timeOwner enum and returns it's string equivalent
std::string ownerStr(const timeOwners timeOwner);

// Generic file reader used to read time mode, time owner,
// host time and host offset
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

// Generic file writer used to write time mode, time owner,
// host time and host offset
template <typename T>
int writeData(const char* fileName, T data)
{
    std::ofstream file(fileName, std::ios::out);
    file << data;
    file.close();
    return 0;
}

// Takes a system setting parameter and returns its value
// provided the specifier is a string.
std::string getSystemSettings(const std::string);

// Reads the data hosted by /org/openbmc/control/power0
std::string getPowerSetting(const std::string);

// Updates .network file with UseNtp=
int updateNetworkSettings(const std::string);

// Reads all the saved data from the last run
int readPersistentData();
#endif
