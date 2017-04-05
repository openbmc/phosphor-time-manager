#pragma once

namespace phosphor
{
namespace time
{
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
}
}

