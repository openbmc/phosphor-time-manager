#include "host_epoch.hpp"

#include <phosphor-logging/log.hpp>

#include <fstream>

namespace phosphor
{
namespace time
{
using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

HostEpoch::HostEpoch(sdbusplus::bus::bus& bus,
                     const char* objPath)
    : EpochBase(bus, objPath),
      offset(readData<decltype(offset)::rep>(offsetFile))
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

    // TODO: implement the logic of setting host time
    // based on timeOwner and timeMode

    auto time = std::chrono::microseconds(value);
    offset = time - getTime();

    // Store the offset to file
    writeData(offsetFile, offset.count());

    server::EpochTime::elapsed(value);
    return value;
}

template <typename T>
T HostEpoch::readData(const char* fileName)
{
    T data{};
    std::ifstream fs(fileName);
    if (fs.is_open())
    {
        fs >> data;
    }
    return data;
}

template <typename T>
void HostEpoch::writeData(const char* fileName, T&& data)
{
    std::ofstream fs(fileName, std::ios::out);
    if (fs.is_open())
    {
        fs << std::forward<T>(data);
    }
}

} // namespace time
} // namespace phosphor

