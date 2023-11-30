#include "settings.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/lg2.hpp>

namespace settings
{

PHOSPHOR_LOG2_USING;

using namespace phosphor::time::utils;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

Objects::Objects(sdbusplus::bus_t& bus)
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
        for (const auto& serviceIter : iter.second)
        {
            for (const Interface& interface : serviceIter.second)
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
