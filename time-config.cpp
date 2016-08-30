#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mapper.h>
#include "time-manager.hpp"

std::map<std::string, TimeConfig::FUNCTOR> TimeConfig::iv_TimeParams = {
    {   "time_mode", std::make_tuple(&TimeConfig::getSystemSettings,
        &TimeConfig::updateTimeMode)
    },

    {   "time_owner", std::make_tuple(&TimeConfig::getSystemSettings,
        &TimeConfig::updateTimeOwner)
    },

    {   "use_dhcp_ntp", std::make_tuple(&TimeConfig::getSystemSettings,
        &TimeConfig::updateNetworkSettings)
    }
};

TimeConfig::TimeConfig() :
    iv_CurrTimeMode(timeModes::NTP),
    iv_RequestedTimeMode(timeModes::NTP),
    iv_CurrTimeOwner(timeOwners::BMC),
    iv_RequestedTimeOwner(timeOwners::BMC),
    iv_CurrDhcpNtp("yes"),
    iv_SettingChangeAllowed(false),
    iv_SplitModeChanged(false),
    iv_dbus(nullptr)
{
    // Not really having anything to do here.
}

// Given a mode string, returns it's equivalent mode enum
TimeConfig::timeModes TimeConfig::getTimeMode(const char* timeMode)
{
    // We are forcing the values to be in specific case and range
    if (!strcmp(timeMode,"NTP"))
    {
        return timeModes::NTP;
    }
    else
    {
        return timeModes::MANUAL;
    }
}

// Accepts a timeMode enum and returns it's string value
const char* TimeConfig::modeStr(const TimeConfig::timeModes& timeMode)
{
    if (timeMode == timeModes::NTP)
    {
        return "NTP";
    }
    else
    {
        return "MANUAL";
    }
}

// Given a owner string, returns it's equivalent owner enum
TimeConfig::timeOwners TimeConfig::getTimeOwner(const char* timeOwner)
{
    if (!strcmp(timeOwner,"BMC"))
    {
        return timeOwners::BMC;
    }
    else if (!strcmp(timeOwner,"HOST"))
    {
        return timeOwners::HOST;
    }
    else if (!strcmp(timeOwner,"SPLIT"))
    {
        return timeOwners::SPLIT;
    }
    else
    {
        return timeOwners::BOTH;
    }
}

// Accepts a timeOwner enum and returns it's string value
const char* TimeConfig::ownerStr(const timeOwners& timeOwner)
{
    if (timeOwner == timeOwners::BMC)
    {
        return "BMC";
    }
    else if (timeOwner == timeOwners::HOST)
    {
        return "HOST";
    }
    else if (timeOwner == timeOwners::SPLIT)
    {
        return "SPLIT";
    }
    else
    {
        return "BOTH";
    }
}

// Returns the busname that hosts objPath
std::unique_ptr<char*> TimeConfig::getProvider(const char* objPath)
{
    char *provider = nullptr;
    mapper_get_service(iv_dbus, objPath, &provider);
    return std::make_unique<char*>(provider);
}

// Accepts a settings name and returns its value.
// for the variant of type 'string' now.
std::string TimeConfig::getSystemSettings(const char* key)
{
    constexpr auto settingsObj = "/org/openbmc/settings/host0";
    constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";
    constexpr auto hostIntf = "org.openbmc.settings.Host";

    const char* value = nullptr;
    std::string settingsVal {};
    sd_bus_message* reply = nullptr;

    std::cout <<"Getting System Settings: " << key << std::endl;

    // Get the provider from object mapper
    auto settingsProvider = getProvider(settingsObj);
    if (!settingsProvider)
    {
        std::cerr << "Error Getting service for Settings" << std::endl;
        return value;
    }

    auto r = sd_bus_call_method(iv_dbus,
                                *settingsProvider,
                                settingsObj,
                                propertyIntf,
                                "Get",
                                nullptr,
                                &reply,
                                "ss",
                                hostIntf,
                                key);
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
    return settingsVal;
}

// Reads value from /org/openbmc/control/power0
// This signature on purpose to plug into time parameter map
std::string TimeConfig::getPowerSetting(const char* key)
{
    constexpr auto powerObj = "/org/openbmc/control/power0";
    constexpr auto powerIntf = "org.openbmc.control.Power";
    constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";

    auto value = -1;
    std::string powerValue {};
    sd_bus_message* reply = nullptr;

    std::cout <<"Reading Power Control key: " << key << std::endl;

    // Get the provider from object mapper
    auto powerProvider = getProvider(powerObj);
    if (!powerProvider)
    {
        std::cerr <<" Error getting provider for Power Settings" << std::endl;
        return powerValue;
    }

    auto r = sd_bus_call_method(iv_dbus,
                                *powerProvider,
                                powerObj,
                                propertyIntf,
                                "Get",
                                nullptr,
                                &reply,
                                "ss",
                                powerIntf,
                                key);
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
        // For maintenance
        goto finish;
    }
finish:
    if (value != -1)
    {
        powerValue = std::to_string(value);
    }
    reply = sd_bus_message_unref(reply);
    return powerValue;
}

// Updates .network file with UseNtp=
int TimeConfig::updateNetworkSettings(const std::string& useDhcpNtp)
{
    constexpr auto networkObj = "/org/openbmc/NetworkManager/Interface";
    constexpr auto networkIntf = "org.openbmc.NetworkManager";

    std::cout << "use_dhcp_ntp = " << useDhcpNtp.c_str() << std::endl;

    // If what we have already is what it is, then just return.
    if (iv_CurrDhcpNtp == useDhcpNtp)
    {
        return 0;
    }

    // Get the provider from object mapper
    auto networkProvider = getProvider(networkObj);
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

        auto newDhcpNtp = useDhcpNtp;
        r = writeData<decltype(newDhcpNtp)>(iv_DhcpNtpFile,
                                            std::move(newDhcpNtp));
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
    for (auto& iter : iv_TimeParams)
    {
        // Get the settings value for various keys.
        auto reader = std::get<READER>(iter.second);
        auto value = (*this.*reader)(iter.first.c_str());
        if (!value.empty())
        {
            // Get the value for the key and validate.
            auto updater = std::get<UPDATER>(iter.second);
            auto r = (*this.*updater)(value);
            if (r < 0)
            {
                std::cerr << "Error setting up initial keys" << std::endl;
                return r;
            }
        }
        else
        {
            std::cerr << "key " << iter.first
                      <<" has no value: " << std::endl;
            return -1;
        }
    }

    // Now that we have taken care of consuming, push this as well
    // so that we can use the same map for handling pgood change too.
    auto readerUpdater = std::make_tuple(&TimeConfig::getPowerSetting,
                                         &TimeConfig::processPgoodChange);
    iv_TimeParams.emplace("pgood", readerUpdater);

    return 0;
}

// This is called by Property Change handler on the event of
// receiving notification on property value change.
int TimeConfig::updatePropertyVal(const char* key, const std::string& value)
{
    auto iter = iv_TimeParams.find(key);
    if (iter != iv_TimeParams.end())
    {
        auto updater = std::get<UPDATER>(iter->second);
        return (*this.*updater)(value);
    }
    // Coming here indicates that we never had a matching key.
    return -1;
}

// Called by sd_event when Properties are changed in Control/power0
// Interested in change to 'pgood'
int TimeConfig::processPgoodChange(const std::string& newPgood)
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
        std::cout <<"Changing time settings is *deferred* now" << std::endl;
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
int TimeConfig::updateTimeMode(const std::string& newModeStr)
{
    auto r = 0;
    iv_RequestedTimeMode = getTimeMode(newModeStr.c_str());

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
                  << newModeStr << " :: " << modeStr(iv_CurrTimeMode) << std::endl;

        // Need this when we either restart or come back from reset
        r = writeData<decltype(modeStr(iv_CurrTimeMode))>
            (iv_TimeModeFile,
             std::move(modeStr(iv_CurrTimeMode)));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it"
                  << std::endl;
    }
    return r;
}

// Manipulates time owner if the system setting allows it
int TimeConfig::updateTimeOwner(const std::string& newOwnerStr)
{
    int r = 0;
    iv_RequestedTimeOwner = getTimeOwner(newOwnerStr.c_str());

    // Needed when owner changes to HOST
    std::string manualMode = "Manual";

    // Needed by time manager to do some house keeping
    iv_SplitModeChanged = false;

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
            iv_SplitModeChanged = true;
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
        r = writeData<decltype(ownerStr(iv_CurrTimeOwner))>
            (iv_TimeOwnerFile,
             std::move(ownerStr(iv_CurrTimeOwner)));
    }
    else
    {
        std::cout <<"Deferring update until system state allows it"
                  << std::endl;
    }

    return r;
}

// Accepts the time mode and makes necessary changes to timedate1
int TimeConfig::modifyNtpSettings(const timeModes& newTimeMode)
{
    auto ntpChangeOp = 0;

    // Pass '1' -or- '0' to SetNTP method indicating Enable/Disable
    ntpChangeOp = (newTimeMode == timeModes::NTP) ? 1 : 0;

    std::cout <<"Applying NTP setting..." << std::endl;

    auto r = sd_bus_call_method(iv_dbus,
                                "org.freedesktop.timedate1",
                                "/org/freedesktop/timedate1",
                                "org.freedesktop.timedate1",
                                "SetNTP",
                                nullptr,
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
    auto savedTimeMode = readData<std::string>(iv_TimeModeFile);
    if (!savedTimeMode.empty())
    {
        iv_CurrTimeMode = getTimeMode(savedTimeMode.c_str());
        std::cout <<"Last known time_mode: "
                  << savedTimeMode.c_str() << std::endl;
    }

    auto savedTimeOwner = readData<std::string>(iv_TimeOwnerFile);
    if (!savedTimeOwner.empty())
    {
        iv_CurrTimeOwner = getTimeOwner(savedTimeOwner.c_str());
        std::cout <<"Last known time_owner: "
                  << savedTimeOwner.c_str() << std::endl;
    }

    auto savedDhcpNtp = readData<std::string>(iv_DhcpNtpFile);
    if (!savedDhcpNtp.empty())
    {
        iv_CurrDhcpNtp = savedDhcpNtp;
        std::cout <<"Last known use_dhcp_ntp: "
                  << iv_CurrDhcpNtp.c_str() << std::endl;
    }
    else
    {
        // This seems to be the first time.
        std::cerr <<"Empty DhcpNtp string" << std::endl;
        iv_CurrDhcpNtp = "yes";
    }

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
