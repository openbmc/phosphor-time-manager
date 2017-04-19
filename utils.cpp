#include "utils.hpp"

#include <phosphor-logging/log.hpp>


namespace phosphor
{
namespace time
{

namespace // anonymous
{
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";

/** @brief The map that maps the string to Mode */
const std::map<std::string, Mode> modeMap =
{
    { "NTP", Mode::NTP },
    { "MANUAL", Mode::MANUAL },
};

/** @brief The map that maps the string to Owner */
const std::map<std::string, Owner> ownerMap =
{
    { "BMC", Owner::BMC },
    { "HOST", Owner::HOST },
    { "SPLIT", Owner::SPLIT },
    { "BOTH", Owner::BOTH },
};
}

namespace utils
{

using namespace phosphor::logging;

std::string getService(sdbusplus::bus::bus& bus,
                       const char* path,
                       const char* interface)
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME,
                                      MAPPER_PATH,
                                      MAPPER_INTERFACE,
                                      "GetObject");

    mapper.append(path, std::vector<std::string>({interface}));
    auto mapperResponseMsg = bus.call(mapper);

    if (mapperResponseMsg.is_method_error())
    {
        // TODO: define repo specific errors and use elog report()
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface));
        return {};
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        // TODO: define repo specific errors and use elog report()
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", path),
                        entry("INTERFACE=%s", interface));
        return {};
    }

    return mapperResponse.begin()->first;
}

Mode strToMode(const std::string& mode)
{
    auto it = modeMap.find(mode);
    if (it == modeMap.end())
    {
        log<level::ERR>("Unrecognized mode",
                        entry("%s", mode.c_str()));
        // TODO: use elog to throw exceptions
        assert(0);
    }
    return it->second;
}

Owner strToOwner(const std::string& owner)
{
    auto it = ownerMap.find(owner);
    if (it == ownerMap.end())
    {
        log<level::ERR>("Unrecognized owner",
                        entry("%s", owner.c_str()));
        // TODO: use elog to throw exceptions
        assert(0);
    }
    return it->second;
}

const char* modeToStr(Mode mode)
{
    const char* ret;
    switch (mode)
    {
    case Mode::NTP:
        ret = "NTP";
        break;
    case Mode::MANUAL:
        ret = "MANUAL";
        break;
    default:
        ret = "Unknown";
        break;
    }
    return ret;
}

const char* ownerToStr(Owner owner)
{
    const char* ret;
    switch (owner)
    {
    case Owner::BMC:
        ret = "BMC";
        break;
    case Owner::HOST:
        ret = "HOST";
        break;
    case Owner::SPLIT:
        ret = "SPLIT";
        break;
    case Owner::BOTH:
        ret = "BOTH";
        break;
    default:
        ret = "Unknown";
        break;
    }
    return ret;
}

} // namespace utils
} // namespace time
} // namespace phosphor
