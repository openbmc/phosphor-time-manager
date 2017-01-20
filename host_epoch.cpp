#include "host_epoch.hpp"
#include "utils.hpp"

#include <log.hpp>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

HostEpoch::HostEpoch(sdbusplus::bus::bus& bus,
                     const char* objPath,
                     Manager* manager)
    : EpochBase(bus, objPath, manager),
      offset(utils::readData<decltype(offset)::rep>(offsetFile))
{
    // Empty
}

uint64_t HostEpoch::elapsed() const
{
    // It does not needs to check owner when getting time
    return (getTime() + offset).count();
}

uint64_t HostEpoch::elapsed(uint64_t value)
{
    if (timeOwner == Owner::BMC)
    {
        log<level::ERR>("Setting HostTime in BMC owner is not allowed");
        // TODO: throw NotAllowed exception
        return 0;
    }

    auto time = std::chrono::microseconds(value);
    offset = time - getTime();

    // Store the offset to file
    utils::writeData<decltype(offset)::rep>(offsetFile, offset.count());

    server::EpochTime::elapsed(value);
    return value;
}

}
}

