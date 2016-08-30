#include <string.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <mapper.h>
#include "time-utils.hpp"

// Defined in time-manager.c
extern sd_bus* gTimeBus;
extern timeMode_t gCurrTimeMode;
extern timeOwner_t gCurrTimeOwner;

// Given a mode string, returns it's equivalent mode enum
timeMode_t getTimeMode(const char* timeMode)
{
    timeMode_t newTimeMode = gCurrTimeMode;

    if (!strcasecmp(timeMode, "NTP"))
    {
        newTimeMode = NTP;
    }
    else if (!strcasecmp(timeMode, "Manual"))
    {
        newTimeMode = MANUAL;
    }

    return newTimeMode;
}

// Accepts a timeMode enum and returns it's string value
const char* modeStr(const timeMode_t timeMode)
{
    const char* modeStr = NULL;

    switch (timeMode)
    {
        case NTP:
            modeStr = "NTP";
            break;
        case MANUAL:
            modeStr = "Manual";
            break;
        default:
            modeStr = "INVALID";
            break;
    }
    return modeStr;
}

// Given a owner string, returns it's equivalent owner enum
timeOwner_t getTimeOwner(const char* timeOwner)
{
    timeOwner_t newTimeOwner = gCurrTimeOwner;

    if (!strcasecmp(timeOwner, "BMC"))
    {
        newTimeOwner = BMC;
    }
    else if (!strcasecmp(timeOwner, "Host"))
    {
        newTimeOwner = HOST;
    }
    else if (!strcasecmp(timeOwner, "Split"))
    {
        newTimeOwner = SPLIT;
    }
    else if (!strcasecmp(timeOwner, "Both"))
    {
        newTimeOwner = BOTH;
    }

    return newTimeOwner;
}

// Accepts a timeOwner enum and returns it's string value
const char* ownerStr(const timeOwner_t timeOwner)
{
    const char* ownerStr = NULL;

    switch (timeOwner)
    {
        case BMC:
            ownerStr = "BMC";
            break;
        case HOST:
            ownerStr = "HOST";
            break;
        case SPLIT:
            ownerStr = "SPLIT";
            break;
        case BOTH:
            ownerStr = "BOTH";
            break;
        default:
            ownerStr = "INVALID";
            break;
    }
    return ownerStr;
}

// Generic file reader
int readData(const char* fileName, void* buffer, size_t len)
{
    FILE* fp = NULL;
    int data = 0;

     // When we first start the daemon, its expected to
     // get a failure.. So create a file
    if (access(fileName, F_OK) == -1)
    {
        fp = fopen(fileName, "w");
        if (fp == NULL)
        {
            fprintf(stderr, "Error creating [%s]\n", fileName);
            return -1;
        }

        /* Default to 0 */
        data = 0;
        fwrite(&data, sizeof(data), 1, fp);
        memcpy(buffer, &data, sizeof(data));
        fclose(fp);
        return 0;
    }

    // This must be a re-run
    fp = fopen(fileName, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening [%s]\n", fileName);
        return -1;
    }

    if (fread(buffer, len, 1, fp) != 1)
    {
        fprintf(stderr, "Error reading [%s]\n", fileName);
        fclose(fp);
        return -1;
    }

    return 0;
}

// Generic file writer
int writeData(const char* fileName, void* buffer, size_t len)
{
    int r = 0;
    FILE* fp = NULL;

    fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening [%s]\n", fileName);
        return -1;
    }

    r = (fwrite(buffer, len, 1, fp) != 1) ? -1 : 0;
    fclose(fp);
    return r;
}

// Returns the busname that hosts obj_path
char* getProvider(const char* objPath)
{
    int r = 0;
    char* provider = NULL;

    r = mapper_get_service(gTimeBus, objPath, &provider);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] getting bus name for [%s] provider\n",
                strerror(-r), objPath);
    }
    return provider;
}

// Accepts a settings name and returns its value. It only works
// for the variant of type 'string' now.
char* getSystemSettings(const char* userSetting)
{
    int r = -1;
    const char* settingsHostObj = "/org/openbmc/settings/host0";
    const char* propertyIntf = "org.freedesktop.DBus.Properties";
    const char* hostIntf = "org.openbmc.settings.Host";

    const char* value = NULL;
    char* settingsValue = NULL;

    // Get the provider from object mapper
    char* settingsProvider = NULL;

    sd_bus_message* reply = NULL;
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    settingsProvider = getProvider(settingsHostObj);
    if (!settingsProvider)
    {
        return settingsProvider;
    }

    r = sd_bus_call_method(gTimeBus,
                           settingsProvider,
                           settingsHostObj,
                           propertyIntf,
                           "Get",
                           &busError,
                           &reply,
                           "ss",
                           hostIntf,
                           userSetting);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] reading system settings\n",
                strerror(-r));
        goto finish;
    }

    r = sd_bus_message_read(reply, "v", "s", &value);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] parsing settings data\n",
                strerror(-r));
    }

finish:
    if (value)
    {
        settingsValue = strdup(value);
    }
    reply = sd_bus_message_unref(reply);
    sd_bus_error_free(&busError);

    free(settingsProvider);
    return settingsValue;
}

// Reads PGOOD value from /org/openbmc/control/power0
int getPgoodValue()
{
    int r = 0;
    const char* pgoodObj = "/org/openbmc/control/power0";
    const char* propertyIntf = "org.freedesktop.DBus.Properties";
    const char* pgoodIintf = "org.openbmc.control.Power";

    return 0;
    int pgood = -1;

    // Get the provider from object mapper
    char* pgoodProvider = NULL;

    sd_bus_error busError = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = NULL;

    pgoodProvider = getProvider(pgoodObj);
    if (!pgoodProvider)
    {
        return -1;
    }

    r = sd_bus_call_method(gTimeBus,
                           pgoodProvider,
                           pgoodObj,
                           propertyIntf,
                           "Get",
                           &busError,
                           &reply,
                           "ss",
                           pgoodIintf,
                           "pgood");
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] reading pgood value\n",
                strerror(-r));
        goto finish;
    }

    r = sd_bus_message_read(reply, "v", "i", &pgood);
    if (r < 0)
    {
        fprintf(stderr, "Error [%s] parsing pgood data\n",
                strerror(-r));
    }

finish:
    sd_bus_error_free(&busError);
    reply = sd_bus_message_unref(reply);
    free(pgoodProvider);

    return pgood;
}

// Updates .network file with UseNtp=
int updtNetworkSettings(const char* useDhcpNtp)
{
    int r = -1;
    const char* networkObj = "/org/openbmc/NetworkManager/Interface";
    const char* networkIntf = "org.openbmc.NetworkManager";

    // Get the provider from object mapper
    char* networkProvider = NULL;
    sd_bus_error busError = SD_BUS_ERROR_NULL;

    networkProvider = getProvider(networkObj);
    if (!networkProvider)
    {
        return -1;
    }

    r = sd_bus_call_method(gTimeBus,
                           networkProvider,
                           networkObj,
                           networkIntf,
                           "UpdateUseNtpField",
                           &busError,
                           NULL,
                           "s",
                           useDhcpNtp);
    if (r < 0)
    {
        fprintf(stderr, "Error:[%s] updating UseNtp in .network file\n",
                strerror(-r));
    }
    else
    {
        printf("Successfully updated UseNtp=[%s] in .network files\n",
               useDhcpNtp);
    }

    sd_bus_error_free(&busError);
    free(networkProvider);
    return r;
}
