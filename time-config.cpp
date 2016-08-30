#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mapper.h>
#include "time-manager.hpp"

TimeConfig::TimeConfig()
{
    iv_CurrTimeMode = timeModes::NTP;
    iv_RequestedTimeMode = timeModes::NTP;

    iv_CurrTimeOwner = timeOwners::BMC;
    iv_RequestedTimeOwner = timeOwners::BMC;

    // Shoudl timesyncd use the NTP settings that are offerred by DHCP ?
    iv_CurrDhcpNtp = std::string("yes");

    // Is the system state allowing us to apply the requests ?
    iv_SettingChangeAllowed = false;
    iv_ResetHostOffset = false;

    // This would be set when TimeManager calls for
    // processing Initial settings
    iv_dbus = nullptr;
}

// Given a mode string, returns it's equivalent mode enum
TimeConfig::timeModes TimeConfig::getTimeMode(std::string timeMode)
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
std::string TimeConfig::modeStr(const TimeConfig::timeModes timeMode)
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
TimeConfig::timeOwners TimeConfig::getTimeOwner(std::string timeOwner)
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
std::string TimeConfig::ownerStr(const timeOwners timeOwner)
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
char* TimeConfig::getProvider(const char* objPath)
{
    char* provider = nullptr;
    auto r = mapper_get_service(iv_dbus, objPath, &provider);
    if (r < 0)
    {
        std::cerr <<"Error " << strerror(-r)
                  << " getting bus name for:" << objPath << std::endl;
    }
    return provider;
}

// Accepts a settings name and returns its value.
// for the variant of type 'string' now.
std::string TimeConfig::getSystemSettings(std::string key)
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

    auto r = sd_bus_call_method(iv_dbus,
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
std::string TimeConfig::getPowerSetting(const std::string key)
{
    constexpr auto powerObj = "/org/openbmc/control/power0";
    constexpr auto powerIntf = "org.openbmc.control.Power";
    constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";

    sd_bus_error busError = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;

    int32_t value = -1;
    std::string powerValue {};

    std::cout <<"Reading Power Control key: " << key << std::endl;

    // Get the provider from object mapper
    auto powerProvider = std::make_unique<char *>(getProvider(powerObj));
    if (!powerProvider)
    {
        return powerValue;
    }

    auto r = sd_bus_call_method(iv_dbus,
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
    value = 0;
    if (value != -1)
    {
        powerValue = std::to_string(value);
    }
    sd_bus_error_free(&busError);
    reply = sd_bus_message_unref(reply);
    return powerValue;
}

// Updates .network file with UseNtp=
int TimeConfig::updateNetworkSettings(std::string useDhcpNtp)
{
    constexpr auto networkObj = "/org/openbmc/NetworkManager/Interface";
    constexpr auto networkIntf = "org.openbmc.NetworkManager";

    std::cout << "use_dhcp_ntp = " << useDhcpNtp.c_str() << std::endl;

    // If what we have already is what it is, then just return.
    if (!strcasecmp(iv_CurrDhcpNtp.c_str(), useDhcpNtp.c_str()))
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

    auto r = sd_bus_call_method(iv_dbus,
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

    return 0;
}

// Reads the values from 'settingsd' and applies:
// 1) Time Mode
// 2) time Owner
// 3) UseNTP setting
// 4) Pgood
int TimeConfig::processInitialSettings(sd_bus* dbus)
{
    // First call from TimeManager to config manager
    iv_dbus = dbus;

    // Sets up the map
    registerTimeParamHandlers();

    // Read saved info like Who was the owner , what was the mode,
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
        auto value = (*this.*iter.second.first)(iter.first);
        if (!value.empty())
        {
            // Get the value for the key and validate.
            int r =  (*this.*iter.second.second)(value);
            if (r < 0)
            {
                std::cerr << "Error setting up initial keys" << std::endl;
                return r;
            }
        }
        else
        {
            std::cerr << "key with no value: "
                      << iter.first << std::endl;
            return -1;
        }
    }

    // Now that we have taken care of consuming, push this as well
    // so that we can use the same map for handling pgood change too.
    FNCTR fnctr = std::make_pair(&TimeConfig::getPowerSetting,
                                 &TimeConfig::processPgoodChange);
    gTimeParams.emplace("pgood", fnctr);

    return 0;
}


// This is called by Property Change handler on the event of
// receiving notification on property value change.
int TimeConfig::updatePropertyVal(std::string key, std::string value)
{
    for (auto& iter : gTimeParams)
    {
        if (!strcasecmp(key.c_str(), iter.first.c_str()))
        {
            return (*this.*iter.second.second)(value);
        }
    }
    return 0;
}

// Called by sd_event when Properties are changed in Control/power0
// Interested in change to 'pgood'
int TimeConfig::processPgoodChange(std::string newPgood)
{
    // Indicating that we are safe to apply any changes
    if (!newPgood.compare("0"))
    {
        iv_SettingChangeAllowed = true;
        std::cout <<"Changing time settings allowed now" << std::endl;
    }
    else
    {
        iv_SettingChangeAllowed = false;
        std::cout <<"Changing time settings is *NOT* allowed now" << std::endl;
    }

    // if we have had users that changed the time settings
    // when we were not ready yet, do it now.
    if (iv_RequestedTimeOwner != iv_CurrTimeOwner)
    {
        auto r = updateTimeOwner(ownerStr(iv_RequestedTimeOwner));
        if (r < 0)
        {
            std::cerr << "Error updating new time owner" << std::endl;
            return r;
        }
        std::cout << "New Owner is : "
                  << ownerStr(iv_RequestedTimeOwner) << std::endl;
    }

    if (iv_RequestedTimeMode != iv_CurrTimeMode)
    {
        auto r = updateTimeMode(modeStr(iv_RequestedTimeMode));
        if (r < 0)
        {
            std::cerr << "Error updating new time mode" << std::endl;
            return r;
        }
        std::cout <<"New Mode is : "
                  << modeStr(iv_RequestedTimeMode) << std::endl;
    }
    return 0;
}

// Manipulates time owner if the system setting allows it
int TimeConfig::updateTimeMode(std::string newModeStr)
{
    auto r = 0;
    iv_RequestedTimeMode = getTimeMode(newModeStr);

    std::cout <<"Requested_Mode: " << newModeStr
              << " Current_Mode: " << modeStr(iv_CurrTimeMode)
              << std::endl;

    if (iv_RequestedTimeMode == iv_CurrTimeMode)
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
    if (iv_CurrTimeOwner == timeOwners::HOST &&
            iv_RequestedTimeOwner == timeOwners::HOST &&
            iv_RequestedTimeMode == timeModes::NTP)
    {
        std::cout <<"Can not set mode to NTP with HOST as owner"
                  << std::endl;
        return 0;
    }

    if (iv_SettingChangeAllowed)
    {
        r = modifyNtpSettings(iv_RequestedTimeMode);
        if (r < 0)
        {
            std::cerr << "Error changing TimeMode settings"
                      << std::endl;
        }
        else
        {
            iv_CurrTimeMode = iv_RequestedTimeMode;
        }
        std::cout << "Current_Mode changed to: "
                  << newModeStr << std::endl;

        // Need this when we either restart or come back from reset
        r = writeData<std::string>(timeModeFile, modeStr(iv_CurrTimeMode));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it"
                  << std::endl;
    }
    return r;
}

// Manipulates time owner if the system setting allows it
int TimeConfig::updateTimeOwner(const std::string newOwnerStr)
{
    int r = 0;
    iv_RequestedTimeOwner = getTimeOwner(newOwnerStr);

    // Needed when owner changes to HOST
    std::string manualMode = "Manual";

    // Needed by time manager to do some house keeping
    iv_ResetHostOffset = false;

    if (iv_RequestedTimeOwner == iv_CurrTimeOwner)
    {
        std::cout <<"Owner is already set to : "
                  << newOwnerStr << std::endl;
        return 0;
    }

    std::cout <<"Requested_Owner: " << newOwnerStr
              << " Current_Owner: " << ownerStr(iv_CurrTimeOwner)
              << std::endl;

    if (iv_SettingChangeAllowed)
    {
        // If we are transitioning from SPLIT to something else,
        // reset the host offset.
        if (iv_CurrTimeOwner == timeOwners::SPLIT &&
                iv_RequestedTimeOwner != timeOwners::SPLIT)
        {
            // Needed by time manager to do some house keeping
            iv_ResetHostOffset = true;
        }
        iv_CurrTimeOwner = iv_RequestedTimeOwner;
        std::cout << "Current_Owner is now: "
                  << newOwnerStr << std::endl;

        // HOST and NTP are exclusive
        if (iv_CurrTimeOwner == timeOwners::HOST)
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
        r = writeData<std::string>(timeOwnerFile, ownerStr(iv_CurrTimeOwner));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it" << std::endl;
    }

    return r;
}

// Registers the callback handlers to act on property changes
void TimeConfig::registerTimeParamHandlers()
{
    FNCTR fnctr = std::make_pair(&TimeConfig::getSystemSettings,
                                 &TimeConfig::updateTimeMode);
    gTimeParams.emplace("time_mode", fnctr);

    fnctr = std::make_pair(&TimeConfig::getSystemSettings,
                           &TimeConfig::updateTimeOwner);
    gTimeParams.emplace("time_owner", fnctr);

    fnctr = std::make_pair(&TimeConfig::getSystemSettings,
                           &TimeConfig::updateNetworkSettings);
    gTimeParams.emplace("use_dhcp_ntp", fnctr);

    return;
}

// Accepts the time mode and makes necessary changes to timedate1
int TimeConfig::modifyNtpSettings(const timeModes newTimeMode)
{
    auto ntpChangeOp = 0;

    // Error return mechanism
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    // Pass '1' -or- '0' to SetNTP method indicating Enable/Disable
    ntpChangeOp = (newTimeMode == timeModes::NTP) ? 1 : 0;

    std::cout <<"Applying NTP setting..." << std::endl;

    auto r = sd_bus_call_method(iv_dbus,
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

// Reads all the saved data from the last run
int TimeConfig::readPersistentData()
{
    // If we are coming back from a reset reload, then need to
    // read what was the last successful Mode and Owner.
    auto savedTimeMode = readData<std::string>(timeModeFile);
    if (!savedTimeMode.empty())
    {
        iv_CurrTimeMode = getTimeMode(savedTimeMode);
        if (iv_CurrTimeMode == timeModes::INVALID)
        {
            std::cerr << "INVALID mode returned" << std::endl;
            return -1;
        }
    }
    std::cout <<"Last known time_mode: "
              << savedTimeMode.c_str() << std::endl;

    auto savedTimeOwner = readData<std::string>(timeOwnerFile);
    if (!savedTimeOwner.empty())
    {
        iv_CurrTimeOwner = getTimeOwner(savedTimeOwner);
        if (iv_CurrTimeOwner == timeOwners::INVALID)
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
        iv_CurrDhcpNtp = savedDhcpNtp;
    }
    std::cout <<"Last known use_dhcp_ntp: "
              << iv_CurrDhcpNtp.c_str() << std::endl;

    // Doing this here to make sure 'pgood' is read and handled
    // first before anything.
    auto pgood = getPowerSetting("pgood");
    if (!pgood.compare("0"))
    {
        std::cout << "Changing settings *allowed* now" << std::endl;
        iv_SettingChangeAllowed = true;
    }
    return 0;
}
