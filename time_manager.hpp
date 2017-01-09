#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

namespace phosphor
{
namespace time
{

class Manager : public sdbusplus::server::object::object<
                    sdbusplus::xyz::openbmc_project::Time::server::EpochTime>
{

    public:
        Manager(sdbusplus::bus::bus& bus,
                const char* busName,
                const char* objPath);

    private:
        sdbusplus::bus::bus& bus;
};

} // namespace time
} // namespace phosphor
