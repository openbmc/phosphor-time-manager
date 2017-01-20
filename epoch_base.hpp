#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

#include <chrono>

namespace phosphor
{
namespace time
{

/** @class EpochBase
 *  @brief Base class for OpenBMC EpochTime implementation.
 *  @details A base class that implements xyz.openbmc_project.Time.EpochTime
 *  DBus API for epoch time.
 */
class EpochBase : public sdbusplus::server::object::object <
    sdbusplus::xyz::openbmc_project::Time::server::EpochTime >
{
    public:
        friend class TestEpochBase;

        /** @brief Supported time modes
         *  NTP     Time sourced by Network Time Server
         *  MANUAL  User of the system need to set the time
         */
        enum class Mode
        {
            NTP,
            MANUAL,
        };

        /** @brief Supported time owners
         *  BMC     Time source may be NTP or MANUAL but it has to be set natively
         *          on the BMC. Meaning, host can not set the time. What it also
         *          means is that when BMC gets IPMI_SET_SEL_TIME, then its ignored.
         *          similarly, when BMC gets IPMI_GET_SEL_TIME, then the BMC's time
         *          is returned.
         *
         *  HOST    Its only IPMI_SEL_SEL_TIME that will set the time on BMC.
         *          Meaning, IPMI_GET_SEL_TIME and request to get BMC time will
         *          result in same value.
         *
         *  SPLIT   Both BMC and HOST will maintain their individual clocks but then
         *          the time information is stored in BMC. BMC can have either NTP
         *          or MANUAL as it's source of time and will set the time directly
         *          on the BMC. When IPMI_SET_SEL_TIME is received, then the delta
         *          between that and BMC's time is calculated and is stored.
         *          When BMC reads the time, the current time is returned.
         *          When IPMI_GET_SEL_TIME is received, BMC's time is retrieved and
         *          then the delta offset is factored in prior to returning.
         *
         *  BOTH:   BMC's time is set with whoever that sets the time. Similarly,
         *          BMC's time is returned to whoever that asks the time.
         */
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
        /** @brief Persistent sdbusplus DBus connection */
        sdbusplus::bus::bus& bus;

        /** @brief The current time mode */
        Mode timeMode;

        /** @brief The current time owner */
        Owner timeOwner;

        /** @brief Set current time to system
         *
         * This function set the time to system by invoking systemd
         * org.freedesktop.timedate1's SetTime method.
         *
         * @param[in] timeOfDayUsec - Microseconds since UTC
         */
        void setTime(const std::chrono::microseconds& timeOfDayUsec);

        /** @brief Get current time
         *
         * @return Microseconds since UTC
         */
        std::chrono::microseconds getTime() const;

        /** @brief Convert a string to enum Mode
         *
         * Convert the time mode string to enum.
         * Valid strings are "NTP", "MANUAL"
         * If it's not a valid time mode string, return NTP.
         *
         * @param[in] mode - The string of time mode
         *
         * @return The Mode enum
         */
        static Mode convertToMode(const std::string& mode);

        /** @brief Convert a string to enum Owner
         *
         * Convert the time owner string to enum.
         * Valid strings are "BMC", "HOST", "SPLIT", "BOTH"
         * If it's not a valid time owner string, return BMC.
         *
         * @param[in] owner - The string of time owner
         *
         * @return The Owner enum
         */
        static Owner convertToOwner(const std::string& owner);

    private:
        /** @brief The initialization function */
        void initialize();

        /** @brief Set current time mode based on the string */
        void setCurrentTimeMode(const std::string& mode);

        /** @brief Set current time owner based on the string */
        void setCurrentTimeOwner(const std::string& owner);

        /** @brief Get setting value from settings manager
         *
         * @param[in] setting - The string of the setting to get
         *
         * @return The value of the setting
         */
        std::string getSettings(const char* setting) const;

        /** @brief The map maps the string key to enum Owner */
        static const std::map<std::string, Owner> ownerMap;
};

} // namespace time
} // namespace phosphor
