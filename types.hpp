#pragma once

#include <xyz/openbmc_project/Time/Owner/server.hpp>
#include <xyz/openbmc_project/Time/Synchronization/server.hpp>

namespace phosphor
{
namespace time
{

    using OwnerSetting = sdbusplus::xyz::openbmc_project::Time::server::Owner;
    using ModeSetting = sdbusplus::xyz::openbmc_project::Time::server::Synchronization;
    using Owner = OwnerSetting::Owners;
    using Mode = ModeSetting::Method;
}
}

