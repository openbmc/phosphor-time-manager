#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <systemd/sd-bus.h>
#include <mapper.h>
#include "time-utils.hpp"

// Defined in time-manager.c
extern sd_bus* gTimeBus;
extern std::string gCurrDhcpNtp;

// Given a mode string, returns it's equivalent mode enum
timeModes getTimeMode(std::string timeMode)
{
    timeModes newTimeMode = timeModes::INVALID;
    if (!strcasecmp(timeMode.c_str(), "NTP"))
    {
        newTimeMode = timeModes::NTP;
    }
    else if (!strcasecmp(timeMode.c_str(), "Manual"))
    {
        newTimeMode = timeModes::MANUAL;
    }
    return newTimeMode;
}

// Accepts a timeMode enum and returns it's string value
std::string modeStr(const timeModes timeMode)
{
    std::string modeStr {};
    switch (timeMode)
    {
    case timeModes::NTP:
        modeStr = "NTP";
        break;
    case timeModes::MANUAL:
        modeStr = "Manual";
        break;
    default:
        modeStr = "INVALID";
        break;
    }
    return modeStr;
}

// Given a owner string, returns it's equivalent owner enum
timeOwners getTimeOwner(std::string timeOwner)
{
    timeOwners newTimeOwner = timeOwners::INVALID;
    if (!strcasecmp(timeOwner.c_str(), "BMC"))
    {
        newTimeOwner = timeOwners::BMC;
    }
    else if (!strcasecmp(timeOwner.c_str(), "Host"))
    {
        newTimeOwner = timeOwners::HOST;
    }
    else if (!strcasecmp(timeOwner.c_str(), "Split"))
    {
        newTimeOwner = timeOwners::SPLIT;
    }
    else if (!strcasecmp(timeOwner.c_str(), "Both"))
    {
        newTimeOwner = timeOwners::BOTH;
    }
    return newTimeOwner;
}

// Accepts a timeOwner enum and returns it's string value
std::string ownerStr(const timeOwners timeOwner)
{
    std::string ownerStr {};
    switch (timeOwner)
    {
    case timeOwners::BMC:
        ownerStr = "BMC";
        break;
    case timeOwners::HOST:
        ownerStr = "HOST";
        break;
    case timeOwners::SPLIT:
        ownerStr = "SPLIT";
        break;
    case timeOwners::BOTH:
        ownerStr = "BOTH";
        break;
    default:
        ownerStr = "INVALID";
        break;
    }
    return ownerStr;
}

// Returns the busname that hosts objPath
char* getProvider(const char* objPath)
{
    char* provider = nullptr;
    auto r = mapper_get_service(gTimeBus, objPath, &provider);
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  << " getting bus name for:" << objPath << std::endl;
    }
    return provider;
}

// Accepts a settings name and returns its value.
// for the variant of type 'string' now.
std::string getSystemSettings(std::string key)
{
    constexpr auto settingsObj = "/org/openbmc/settings/host0";
    constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";
    constexpr auto hostIntf = "org.openbmc.settings.Host";

    const char* value = nullptr;
    std::string settingsVal {};

    sd_bus_message* reply = nullptr;
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    std::cout <<"Getting System Settings: " << key << std::endl;

    // Get the provider from object mapper
    auto settingsProvider = std::make_unique<char*>
                            (getProvider(settingsObj));
    if (!settingsProvider)
    {
        return value;
    }

    auto r = sd_bus_call_method(gTimeBus,
                                *settingsProvider,
                                settingsObj,
                                propertyIntf,
                                "Get",
                                &busError,
                                &reply,
                                "ss",
                                hostIntf,
                                key.c_str());
    if (r < 0)
    {
        std::cerr <<"Error" << strerror(-r)
                  <<" reading system settings" << std::endl;
        goto finish;
    }

    r = sd_bus_message_read(reply, "v", "s", &value);
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  <<" parsing settings data" << std::endl;
    }
finish:
    if (value)
    {
        settingsVal.assign(value);
    }
    reply = sd_bus_message_unref(reply);
    sd_bus_error_free(&busError);
    return settingsVal;
}

// Reads value from /org/openbmc/control/power0
// This signature on purpose to plug into time parameter map
std::string getPowerSetting(const std::string key)
{
    //constexpr auto powerObj = "/org/openbmc/control/power0";
    //constexpr auto powerIntf = "org.openbmc.control.Power";

    constexpr auto powerObj = "/org/openbmc/settings/host0";
    constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";
    constexpr auto powerIntf = "org.openbmc.settings.Host";

    sd_bus_error busError = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    int32_t value = -1;
    std::string powerValue {};

    std::cout <<" Reading Power Control key: " << key << std::endl;

    // Get the provider from object mapper
    auto powerProvider = std::make_unique<char *>(getProvider(powerObj));
    if (!powerProvider)
    {
        return powerValue;
    }

    auto r = sd_bus_call_method(gTimeBus,
                                *powerProvider,
                                powerObj,
                                propertyIntf,
                                "Get",
                                &busError,
                                &reply,
                                "ss",
                                powerIntf,
                                key.c_str());
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  << "reading: " << key << std::endl;
        goto finish;
    }

    r = sd_bus_message_read(reply, "v", "i", &value);
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  <<" parsing " << key << "value" << std::endl;
    }
finish:
    if (value != -1)
    {
        powerValue = std::to_string(value);
    }
    sd_bus_error_free(&busError);
    reply = sd_bus_message_unref(reply);
    return powerValue;
}

// Updates .network file with UseNtp=
int updateNetworkSettings(std::string useDhcpNtp)
{
    constexpr auto networkObj = "/org/openbmc/NetworkManager/Interface";
    constexpr auto networkIntf = "org.openbmc.NetworkManager";

    // If what we have already is what it is, then just return.
    if (!strcasecmp(gCurrDhcpNtp.c_str(), useDhcpNtp.c_str()))
    {
        return 0;
    }

    // Get the provider from object mapper
    auto networkProvider = std::make_unique<char*>
                           (getProvider(networkObj));
    if (!networkProvider)
    {
        return -1;
    }

    auto r = sd_bus_call_method(gTimeBus,
                                *networkProvider,
                                networkObj,
                                networkIntf,
                                "UpdateUseNtpField",
                                nullptr,
                                nullptr,
                                "s",
                                useDhcpNtp.c_str());
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  << " updating UseNtp" << std::endl;
    }
    else
    {
        std::cout <<"Successfully updated UseNtp=["
                  << useDhcpNtp << "]" << std::endl;

        r = writeData<std::string>(dhcpNtpFile, useDhcpNtp);
    }
    return r;
}
