#include "manager.hpp"

#include "utils.hpp"

#include "VariantVisitors.hpp"
//#include "bmc_epoch.hpp"


#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <sstream>
#include <chrono>

#define RESP_DATA_INDEX 5
#define RESP_DATA_LENGTH  11

namespace rules = sdbusplus::bus::match::rules;

namespace // anonymous
{

constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_NTP = "SetNTP";
constexpr auto METHOD_SET_TIME = "SetTime";
const constexpr char* entityManagerName = "xyz.openbmc_project.EntityManager";
constexpr const char* configInterface =
    "xyz.openbmc_project.Configuration.IpmbCellInfo";
} // namespace

namespace phosphor
{
namespace time
{

using namespace phosphor::logging;
using namespace std::chrono;
using sdbusplus::exception::SdBusError;

Manager::Manager(sdbusplus::bus::bus& bus) : bus(bus), settings(bus)
{
    using namespace sdbusplus::bus::match::rules;
    settingsMatches.emplace_back(
        bus, propertiesChanged(settings.timeSyncMethod, settings::timeSyncIntf),
        std::bind(std::mem_fn(&Manager::onSettingsChanged), this,
                  std::placeholders::_1));
   
    // Check the settings daemon to process the new settings
    auto mode = getSetting(settings.timeSyncMethod.c_str(),
                           settings::timeSyncIntf, PROPERTY_TIME_MODE);

    onPropertyChanged(PROPERTY_TIME_MODE, mode);      
}

void Manager::onPropertyChanged(const std::string& key,
                                const std::string& value)
{
    assert(key == PROPERTY_TIME_MODE);

    // Notify listeners
    setCurrentTimeMode(value);
    onTimeModeChanged(value);
}

int Manager::onSettingsChanged(sdbusplus::message::message& msg)
{
    using Interface = std::string;
    using Property = std::string;
    using Value = std::string;
    using Properties = std::map<Property, std::variant<Value>>;

    Interface interface;
    Properties properties;

    msg.read(interface, properties);

    for (const auto& p : properties)
    {
        onPropertyChanged(p.first, std::get<std::string>(p.second));
    }

    return 0;
}

void Manager::updateNtpSetting(const std::string& value)
{
    bool isNtp =
        (value == "xyz.openbmc_project.Time.Synchronization.Method.NTP");
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE, SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE, METHOD_SET_NTP);
    method.append(isNtp, false); // isNtp: 'true/false' means Enable/Disable
                                 // 'false' meaning no policy-kit

    try
    {
        bus.call_noreply(method);
        log<level::INFO>("Updated NTP setting", entry("ENABLED=%d", isNtp));
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Failed to update NTP setting",
                        entry("ERR=%s", ex.what()));
    }
}

bool Manager::setCurrentTimeMode(const std::string& mode)
{
    auto newMode = utils::strToMode(mode);
    if (newMode != timeMode)
    {
        log<level::INFO>("Time mode is changed",
                         entry("MODE=%s", mode.c_str()));
        timeMode = newMode;
        return true;
    }
    else
    {
        return false;
    }
}

static constexpr auto systemdBusname = "org.freedesktop.systemd1";
static constexpr auto systemdPath = "/org/freedesktop/systemd1";
static constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";
using IpmbMethodType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

void Manager::updateHostSyncSetting(std::vector<uint8_t>& meAddress, uint8_t netFn, uint8_t lun, uint8_t cmd,
                                   std::vector<uint8_t>& cmdData, std::vector<uint8_t>& respData,
                                   std::string bridgeInterface)
{
    if(bridgeInterface == "Ipmb")
    {
        auto method = bus.new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb", "/xyz/openbmc_project/Ipmi/Channel/Ipmb", 
                                          "org.openbmc.Ipmb", "sendRequest");
        try
        {
            for(uint8_t i = 0; i < meAddress.size(); i++)
            {       
                method.append(meAddress[i], netFn, lun, cmd, cmdData);
                auto reply = bus.call(method);
                if (reply.is_method_error())
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>("Error reading from ME");
                    continue;
                }
                IpmbMethodType resp; 
                reply.read(resp);
                respData = std::get<RESP_DATA_INDEX> (resp); 
                if(respData.size() >= RESP_DATA_LENGTH)
                {
                    utils::setTime(bus, microseconds(seconds(parseToEpoch(respData))));
                    break;
                }
            }
        }
        catch(...)
        {
            log<level::ERR>("unable to connect to given hostID");        
        }
    }

}

uint64_t Manager::parseToEpoch(std::vector<uint8_t>& respData)
{
    uint16_t size = respData.size();
    uint16_t last = size-1;
    uint16_t fourthValFrmLast = size-4;
    uint64_t epochTime = 0;
    //fetch last 4 value from response data and concatenate them to make epoch time
    for(uint16_t i=last; i >= fourthValFrmLast; i--)
    {
        if(i != fourthValFrmLast)
            epochTime = (epochTime | respData[i]) << 8;
        else if(i == fourthValFrmLast)
            epochTime = (epochTime | respData[i]);
    }
    return epochTime;            
}

void Manager::onTimeModeChanged(const std::string& mode)
{
    updateNtpSetting(mode); //NTP Mode
    if(mode == "xyz.openbmc_project.Time.Synchronization.Method.HostSync")
    {
        sleep(20);
        uint8_t netFn,cmd,lun;
        std::vector<uint8_t> cmdData,hostData;
        std::string bridgeInterface;
        readFromJson(netFn,lun, cmd, hostData, cmdData, bridgeInterface);
        updateHostSyncSetting(hostData, netFn, lun, cmd, cmdData, respData, bridgeInterface); //HostSync mode
    }       
}

void Manager::readFromJson(uint8_t &netFn, uint8_t &lun, uint8_t &cmd, std::vector<uint8_t> &hostData,
                           std::vector<uint8_t> &cmdData, std::string &bridgeInterface)
{
    unsigned int num = 0;
    auto method = bus.new_method_call(entityManagerName, "/", "org.freedesktop.DBus.ObjectManager",
                                "GetManagedObjects");
    method.append();
    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>("Error contacting entity manager");
        return;
    }
    try
    {      
        ManagedObjectType resp;
        reply.read(resp);
        for (const auto& pathPair : resp)
        { 
            for (const auto& entry : pathPair.second)
            {
                if (entry.first != configInterface)
                {
                    continue;
                }
                std::stringstream dataStream(loadVariant<std::string>(entry.second, "HostId"));
                while(dataStream >> num){
                    hostData.push_back(num);
                }
                num = 0;
                dataStream.str("");
                dataStream.clear();
                netFn = loadVariant<uint8_t>(entry.second, "NetFn");
                lun = loadVariant<uint8_t>(entry.second, "Lun");
                cmd = loadVariant<uint8_t>(entry.second, "Cmd");
                dataStream << (loadVariant<std::string> (entry.second, "CmdData"));
                while(dataStream >> num){
                    cmdData.push_back(num);
                }
                bridgeInterface = loadVariant<std::string>(entry.second, "Interface");          
            }
        }
        
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Error in reading from IPMB cell info",entry("ERR=%s", ex.what()));
    }

}

std::string Manager::getSetting(const char* path, const char* interface,
                                const char* setting) const
{
    std::string settingManager = utils::getService(bus, path, interface);
    return utils::getProperty<std::string>(bus, settingManager.c_str(), path,
                                           interface, setting);
}

} // namespace time
} // namespace phosphor
