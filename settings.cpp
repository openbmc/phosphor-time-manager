#include "settings.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

namespace settings
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperIntf = "xyz.openbmc_project.ObjectMapper";

Objects::Objects(sdbusplus::bus_t& bus) : bus(bus)
{
    std::vector<std::string> settingsIntfs = {timeSyncIntf};
    auto depth = 0;

    auto mapperCall = bus.new_method_call(mapperService, mapperPath, mapperIntf,
                                          "GetSubTree");
    mapperCall.append(root);
    mapperCall.append(depth);
    mapperCall.append(settingsIntfs);

    using Interfaces = std::vector<Interface>;
    using MapperResponse = std::vector<
        std::pair<Path, std::vector<std::pair<Service, Interfaces>>>>;
    MapperResponse result;

    try
    {
        auto response = bus.call(mapperCall);
        response.read(result);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        lg2::error("Failed to invoke GetSubTree method: {ERROR}", "ERROR", ex);
    }

    if (result.empty())
    {
        lg2::error("Invalid response from mapper");
    }

    for (const auto& iter : result)
    {
        const Path& path = iter.first;
        for (const auto& service_iter : iter.second)
        {
            for (const Interface& interface : service_iter.second)
            {
                if (timeSyncIntf == interface)
                {
                    timeSyncMethod = path;
                }
            }
        }
    }
}

} // namespace settings
