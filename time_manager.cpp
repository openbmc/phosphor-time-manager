#include <log.hpp>

#include "time_manager.hpp"

namespace phosphor
{
namespace time
{

using namespace sdbusplus::xyz::openbmc_project::Time;
using namespace phosphor::logging;

Manager::Manager(sdbusplus::bus::bus& bus,
                 const char* /*busName*/,
                 const char* objPath)
    : sdbusplus::server::object::object<EpochTime>(bus, objPath),
      bus(bus)
{
  // Empty
}

} // namespace time
} // namespace phosphor
