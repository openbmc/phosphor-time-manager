#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

#include <sstream>

#define RESP_DATA_INDEX 5
#define RESP_DATA_LENGTH 11

namespace phosphor
{
namespace time
{

namespace // anonymous
{
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
constexpr auto DBUS_PROPERTY_IFACE = "org.freedesktop.DBus.Properties";
const constexpr char* entityManagerName = "xyz.openbmc_project.EntityManager";
constexpr const char* configInterface =
    "xyz.openbmc_project.Configuration.IpmbSELInfo";

} // namespace

namespace utils
{

using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus, const char* path,
                       const char* interface)
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

    mapper.append(path, std::vector<std::string>({interface}));
    try
    {
        auto mapperResponseMsg = bus.call(mapper);

        std::vector<std::pair<std::string, std::vector<std::string>>>
            mapperResponse;
        mapperResponseMsg.read(mapperResponse);
        if (mapperResponse.empty())
        {
            log<level::ERR>("Error reading mapper response");
            throw std::runtime_error("Error reading mapper response");
        }
        if (mapperResponse.size() < 1)
        {
            return "";
        }
        return mapperResponse[0].first;
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Mapper call failed", entry("METHOD=%d", "GetObject"),
                        entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface));
        throw std::runtime_error("Mapper call failed");
    }
}

const std::vector<std::string> getSubTreePaths(sdbusplus::bus::bus& bus,
                                               const std::string& objectPath,
                                               const std::string& interface)
{
    std::vector<std::string> paths;

    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetSubTreePaths");
    method.append(objectPath.c_str());
    method.append(0); // Depth 0 to search all
    method.append(std::vector<std::string>({interface.c_str()}));
    auto reply = bus.call(method);

    reply.read(paths);

    return paths;
}

std::vector<uint8_t> strToIntArray(std::string stream)
{
    unsigned int num = 0;
    std::vector<uint8_t> data;
    std::stringstream dataStream(stream);

    while (dataStream >> num)
    {
        data.push_back(num);
    }

    dataStream.str("");
    dataStream.clear();
    return data;
}

Mode strToMode(const std::string& mode)
{
    return ModeSetting::convertMethodFromString(mode);
}

std::string modeToStr(Mode mode)
{
    return sdbusplus::xyz::openbmc_project::Time::server::convertForMessage(
        mode);
}

constexpr auto SYSTEMD_TIME_SERVICE = "org.freedesktop.timedate1";
constexpr auto SYSTEMD_TIME_PATH = "/org/freedesktop/timedate1";
constexpr auto SYSTEMD_TIME_INTERFACE = "org.freedesktop.timedate1";
constexpr auto METHOD_SET_TIME = "SetTime";

using namespace std::chrono;
bool setTime(sdbusplus::bus::bus& bus, const microseconds& usec)
{
    auto method = bus.new_method_call(SYSTEMD_TIME_SERVICE, SYSTEMD_TIME_PATH,
                                      SYSTEMD_TIME_INTERFACE, METHOD_SET_TIME);
    method.append(static_cast<int64_t>(usec.count()),
                  false,  // relative
                  false); // user_interaction

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Error in setting system time");
        using FailedError =
            sdbusplus::xyz::openbmc_project::Time::Error::Failed;
        using namespace xyz::openbmc_project::Time;
        elog<FailedError>(Failed::REASON(ex.what()));
    }
    return true;
}
#ifdef USE_IPMB_HOST_TIME
using IpmbMethodType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

bool readHostTimeViaIpmb(sdbusplus::bus::bus& bus, ipmbCmdInfo hostTimeCmd,
                         std::vector<uint8_t>& respData)
{
    bool result = false;
    auto method = bus.new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                      "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                      "org.openbmc.Ipmb", "sendRequest");
    log<level::INFO>("TimeManager: updateHostSyncSetting");
    try
    {
        for (auto& ipmbAddr : hostTimeCmd.ipmbAddr)
        {
            method.append(ipmbAddr, hostTimeCmd.netFn, hostTimeCmd.lun,
                          hostTimeCmd.cmd, hostTimeCmd.cmdData);
            auto reply = bus.call(method);
            IpmbMethodType resp;
            reply.read(resp);
            respData = std::get<RESP_DATA_INDEX>(resp);
            if (respData.size() >= RESP_DATA_LENGTH)
            {
                result = true;
                break;
            }
        }
    }
    catch (...)
    {
        log<level::ERR>("unable to connect to given hostID");
    }
    return result;
}

ipmbCmdInfo loadIPMBCmd(sdbusplus::bus::bus& bus)
{
    ipmbCmdInfo hostTimeIpmbCmd;
    std::vector<std::string> objectPath;
    objectPath = getSubTreePaths(bus, "/", configInterface);
    if (objectPath.empty())
    {
        log<level::ERR>("ipmb cmd subtree path is empty");
    }
    std::string IpmbCmdPath = objectPath[0];

    hostTimeIpmbCmd.ipmbAddr = strToIntArray(getProperty<std::string>(
        bus, entityManagerName, IpmbCmdPath.c_str(), configInterface, "Bus"));
    hostTimeIpmbCmd.netFn = static_cast<uint8_t>(getProperty<uint64_t>(
        bus, entityManagerName, IpmbCmdPath.c_str(), configInterface, "Index"));
    hostTimeIpmbCmd.lun = static_cast<uint8_t>(getProperty<uint64_t>(
        bus, entityManagerName, IpmbCmdPath.c_str(), configInterface, "Lun"));
    hostTimeIpmbCmd.cmd = static_cast<uint8_t>(getProperty<uint64_t>(
        bus, entityManagerName, IpmbCmdPath.c_str(), configInterface, "C1"));
    hostTimeIpmbCmd.cmdData = strToIntArray(
        getProperty<std::string>(bus, entityManagerName, IpmbCmdPath.c_str(),
                                 configInterface, "ByteArray"));
    hostTimeIpmbCmd.bridgeInterface =
        getProperty<std::string>(bus, entityManagerName, IpmbCmdPath.c_str(),
                                 configInterface, "Interface");

    return hostTimeIpmbCmd;
}

uint64_t parseToEpoch(std::vector<uint8_t>& respData)
{
    uint16_t size = respData.size();
    uint16_t last = size - 1;
    uint16_t fourthValFrmLast = size - 4;
    uint64_t epochTime = 0;
    // fetch last 4 value from response data and concatenate them to make epoch
    // time
    for (uint16_t i = last; i >= fourthValFrmLast; i--)
    {
        if (i != fourthValFrmLast)
            epochTime = (epochTime | respData[i]) << 8;
        else if (i == fourthValFrmLast)
            epochTime = (epochTime | respData[i]);
    }
    return epochTime;
}

uint64_t getHostTimeViaIpmb(sdbusplus::bus::bus& bus)
{
    /** @brief resonse data from ipmb method call */
    std::vector<uint8_t> respData;

    ipmbCmdInfo hostTimeCmd;

    hostTimeCmd = loadIPMBCmd(bus);
    log<level::INFO>("TimeManager: updateBmcTimeFromHost",
                     entry("NETFN=%d,", hostTimeCmd.netFn),
                     entry("CMD=%d", hostTimeCmd.cmd),
                     entry("LUN=%d", hostTimeCmd.lun));

    bool ipmbResult = readHostTimeViaIpmb(
        bus, hostTimeCmd,
        respData); // send ipmb command to all available hosts and return
                   // after a valid time is read from any host
    if (ipmbResult != true)
    {
        log<level::INFO>("TimeManager: error reading host time via ipmb");
        return 0;
    }
    return parseToEpoch(respData);
}
#endif

uint64_t getTimeFromHost(sdbusplus::bus::bus& bus)
{
    uint64_t hostTime = 0;
    (void)bus;
#ifdef USE_IPMB_HOST_TIME
    hostTime = getHostTimeViaIpmb(bus);
#endif

    return hostTime;
}

void updateBmcTimeFromHost(sdbusplus::bus::bus& bus)
{
    // set the bmc time based on the read host time
    uint64_t bmcTime;
    bmcTime = getTimeFromHost(bus);

    if (bmcTime != 0)
    {
        setTime(bus, microseconds(seconds(bmcTime)));
    }
    else
    {
        log<level::ERR>("updateBmcTimeFromHost : Reading Host time failed");
    }
}

} // namespace utils
} // namespace time
} // namespace phosphor
