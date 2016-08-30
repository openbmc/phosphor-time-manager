#ifndef TIME_UTILS_HPP
#define TIME_UTILS_HPP

#include <unistd.h>
#include <stdio.h>

// Possible time modes
typedef enum
{
    NTP = 0,
    MANUAL,
} timeMode_t;

// Possible time owners
typedef enum
{
    BMC = 0,
    HOST,
    SPLIT,
    BOTH,
} timeOwner_t;

// Given a time mode string, returns the equivalent enum
timeMode_t getTimeMode(const char* timeMode);

// Accepts timeMode enum and returns it's string equivalent
const char* modeStr(const timeMode_t timeMode);

// Given a time owner string, returns the equivalent enum
timeOwner_t getTimeOwner(const char* timeOwner);

// Accepts timeOwner enum and returns it's string equivalent
const char* ownerStr(const timeOwner_t timeOwner);

// Generic file reader used to read time mode, time owner,
// host time and host offset
int readData(const char* fileName, void* buffer, size_t len);

// Generic file writer used to write time mode, time owner,
// host time and host offset
int writeData(const char* fileName, void* buffer, size_t len);

// Takes a system setting parameter and returns its value
// provided the specifier is a string.
char* getSystemSettings(const char* userSetting);

// Reads the PGOOD property from /org/openbmc/control/power0
int getPgoodValue();

// Updates .network file with UseNtp=
int updtNetworkSettings(const char* useDhcpNtp);

// Accepts the dbus path and returns it's provider
char* getProvider(const char* objPath);
#endif
