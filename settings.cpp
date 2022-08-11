#include "settings.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

namespace settings
{

PHOSPHOR_LOG2_USING;

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

constexpr auto mapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperIntf = "xyz.openbmc_project.ObjectMapper";

Objects::Objects(sdbusplus::bus_t& bus) : bus(bus)
{
    Interfaces settingsIntfs = {timeSyncIntf};
    MapperResponse result;

    try
    {
        result = getSubTree(bus, root, settingsIntfs, 0);
    }
    catch (const sdbusplus::exception_t& ex)
    {
        error("Failed to invoke GetSubTree method: {ERROR}", "ERROR", ex);
    }

    if (result.empty())
    {
        error("Invalid response from mapper");
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
