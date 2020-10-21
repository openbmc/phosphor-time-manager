#include "epoch_base.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Time/error.hpp>

#include <iomanip>
#include <sstream>


namespace phosphor
{
namespace time
{

using namespace phosphor::logging;
using FailedError = sdbusplus::xyz::openbmc_project::Time::Error::Failed;

EpochBase::EpochBase(sdbusplus::bus::bus& bus, const char* objPath) :
    sdbusplus::server::object::object<EpochTime>(bus, objPath), bus(bus)
{}

void EpochBase::onModeChanged(Mode mode)
{
    timeMode = mode;
}

using namespace std::chrono;
microseconds EpochBase::getTime() const
{
    auto now = system_clock::now();
    return duration_cast<microseconds>(now.time_since_epoch());
}

} // namespace time
} // namespace phosphor
