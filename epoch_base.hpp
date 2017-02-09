#pragma once

#include "property_change_listener.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{

class EpochBase : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime >,
    public PropertyChangeListner
{
    public:
        friend class TestEpochBase;

        EpochBase(sdbusplus::bus::bus& bus,
                  const char* objPath);
        void onModeChanged(Mode mode) override;
        void onOwnerChanged(Owner owner) override;

    protected:
        sdbusplus::bus::bus& bus;
        Mode timeMode = Mode::NTP;
        Owner timeOwner = Owner::BMC;

        bool setTime(const std::chrono::microseconds& timeOfDayUsec);
        std::chrono::microseconds getTime() const;
};

} // namespace time
} // namespace phosphor
