#include "utils.hpp"

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>


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

using InvalidArgumentError =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;

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
        using namespace xyz::openbmc_project::Time::Internal;
        elog<MethodErr>(MethodError::METHOD_NAME("GetObject"),
                          MethodError::PATH(path),
                          MethodError::INTERFACE(interface),
                          MethodError::MISC({}));
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        using namespace xyz::openbmc_project::Time::Internal;
        elog<MethodErr>(MethodError::METHOD_NAME("GetObject"),
                          MethodError::PATH(path),
                          MethodError::INTERFACE(interface),
                          MethodError::MISC("Error reading mapper response"));
    }

    return mapperResponse.begin()->first;
}

Mode strToMode(const std::string& mode)
{
    auto it = modeMap.find(mode);
    if (it == modeMap.end())
    {
        using namespace xyz::openbmc_project::Common;
        elog<InvalidArgumentError>(
            InvalidArgument::ARGUMENT_NAME("TimeMode"),
            InvalidArgument::ARGUMENT_VALUE(mode.c_str()));
    }
    return it->second;
}

Owner strToOwner(const std::string& owner)
{
    auto it = ownerMap.find(owner);
    if (it == ownerMap.end())
    {
        using namespace xyz::openbmc_project::Common;
        elog<InvalidArgumentError>(
            InvalidArgument::ARGUMENT_NAME("TimeOwner"),
            InvalidArgument::ARGUMENT_VALUE(owner.c_str()));
    }
    return it->second;
}

const char* modeToStr(Mode mode)
{
    const char* ret{};
    switch (mode)
    {
    case Mode::NTP:
        ret = "NTP";
        break;
    case Mode::MANUAL:
        ret = "MANUAL";
        break;
    default:
        // TODO: use elog to throw exceptions
        assert(0);
        break;
    }
    return ret;
}

const char* ownerToStr(Owner owner)
{
    const char* ret{};
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
        // TODO: use elog to throw exceptions
        assert(0);
        break;
    }
    return ret;
}

} // namespace utils
} // namespace time
} // namespace phosphor
