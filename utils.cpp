#include "utils.hpp"

#include "LoadVariant.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

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
const constexpr char* entityManagerName = "xyz.openbmc_project.EntityManager";
constexpr const char* configInterface =
    "xyz.openbmc_project.Configuration.IpmbSelInfo";

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

static constexpr auto systemdBusname = "org.freedesktop.systemd1";
static constexpr auto systemdPath = "/org/freedesktop/systemd1";
static constexpr auto systemdInterface = "org.freedesktop.systemd1.Manager";
using IpmbMethodType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

bool readHostTimeViaIpmb(sdbusplus::bus::bus& bus,
                         std::vector<uint8_t>& meAddress, uint8_t netFn,
                         uint8_t lun, uint8_t cmd,
                         std::vector<uint8_t>& cmdData,
                         std::vector<uint8_t>& respData,
                         std::string bridgeInterface)
{
    bool result = false;
    if (bridgeInterface == "Ipmb")
    {
        auto method =
            bus.new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                "org.openbmc.Ipmb", "sendRequest");
        log<level::INFO>("TimeManager: updateHostSyncSetting");
        try
        {
            for (uint8_t i = 0; i < meAddress.size(); i++)
            {
                method.append(meAddress[i], netFn, lun, cmd, cmdData);
                auto reply = bus.call(method);
                if (reply.is_method_error())
                {
                    phosphor::logging::log<phosphor::logging::level::ERR>(
                        "Error reading from ME");
                    continue;
                }
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
    }
    return result;
}
void loadIPMBCmd(sdbusplus::bus::bus& bus, uint8_t& netFn, uint8_t& lun,
                 uint8_t& cmd, std::vector<uint8_t>& hostData,
                 std::vector<uint8_t>& cmdData, std::string& bridgeInterface)
{
    unsigned int num = 0;
    auto method = bus.new_method_call(entityManagerName, "/",
                                      "org.freedesktop.DBus.ObjectManager",
                                      "GetManagedObjects");
    log<level::INFO>("TimeManager: loadIPMBCmd");
    method.append();
    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error contacting entity manager");
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
                std::stringstream dataStream(
                    loadVariant<std::string>(entry.second, "Bus"));
                while (dataStream >> num)
                {
                    hostData.push_back(num);
                }
                num = 0;
                dataStream.str("");
                dataStream.clear();
                netFn = loadVariant<uint8_t>(entry.second, "Index");
                lun = loadVariant<uint8_t>(entry.second, "Lun");
                cmd = loadVariant<uint8_t>(entry.second, "C1");
                dataStream << (loadVariant<std::string>(entry.second,
                                                        "ByteArray"));
                while (dataStream >> num)
                {
                    cmdData.push_back(num);
                }
                bridgeInterface =
                    loadVariant<std::string>(entry.second, "Interface");
            }
        }
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Error in reading from IPMB cell info",
                        entry("ERR=%s", ex.what()));
    }
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

void updateBmcTimeFromHost(sdbusplus::bus::bus& bus)
{
    log<level::INFO>("TimeManager: updateBmcTimeFromHost");
    uint8_t netFn, cmd, lun;
    std::vector<uint8_t> cmdData, hostData;
    /** @brief resonse data from ipmb method call */
    std::vector<uint8_t> respData;

    std::string bridgeInterface;
    loadIPMBCmd(bus, netFn, lun, cmd, hostData, cmdData,
                bridgeInterface); // todo : struct variable
    log<level::DEBUG>("TimeManager: updateBmcTimeFromHost",
                      entry("netFn=%d,", netFn), entry("cmd=%d", cmd),
                      entry("lun=%d", lun));

    bool ipmbResult = readHostTimeViaIpmb(
        bus, hostData, netFn, lun, cmd, cmdData, respData,
        bridgeInterface); // send ipmb command to all available hosts and return
                          // after a valid time is read from any host

    // set the bmc time based on the read host time
    if (ipmbResult)
    {
        setTime(bus, microseconds(seconds(parseToEpoch(respData))));
    }
    else
    {
        log<level::ERR>("updateBmcTimeFromHost : Reading Host time failed");
    }
}

} // namespace utils
} // namespace time
} // namespace phosphor
