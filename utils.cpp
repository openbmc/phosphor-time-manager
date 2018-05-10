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

    std::vector<std::pair<std::string, std::vector<std::string>>>
        mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        using namespace xyz::openbmc_project::Time::Internal;
        elog<MethodErr>(MethodError::METHOD_NAME("GetObject"),
                          MethodError::PATH(path),
                          MethodError::INTERFACE(interface),
                          MethodError::MISC("Error reading mapper response"));
    }
    if (mapperResponse.size() < 1){
        return "";
    }
    return mapperResponse[0].first;
}

Mode strToMode(const std::string& mode)
{
    return ModeSetting::convertMethodFromString(mode);
}

Owner strToOwner(const std::string& owner)
{
    return OwnerSetting::convertOwnersFromString(owner);
}

std::string modeToStr(Mode mode)
{
    return sdbusplus::xyz::openbmc_project::Time::server::convertForMessage(mode);
}

std::string ownerToStr(Owner owner)
{
    return sdbusplus::xyz::openbmc_project::Time::server::convertForMessage(owner);
}

} // namespace utils
} // namespace time
} // namespace phosphor
