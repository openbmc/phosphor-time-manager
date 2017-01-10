#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{

class EpochBase : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime >
{
    public:
        friend class TestEpochBase;
        enum class Mode
        {
            NTP,
            MANUAL,
        };
        enum class Owner
        {
            BMC,
            HOST,
            SPLIT,
            BOTH,
        };

        EpochBase(sdbusplus::bus::bus& bus,
                  const char* objPath);

    protected:
        sdbusplus::bus::bus& bus;
        Mode timeMode;
        Owner timeOwner;

        bool setTime(const std::chrono::microseconds& timeOfDayUsec);
        std::chrono::microseconds getTime() const;

        static Mode convertToMode(const std::string& value);
        static Owner convertToOwner(const std::string& value);
        static std::string convertToStr(
            const std::chrono::microseconds& us);

    private:
        void initialize();

        void setCurrentTimeMode(const std::string& value);
        void setCurrentTimeOwner(const std::string& value);

        std::string getSettings(const char* value) const;
};

} // namespace time
} // namespace phosphor
