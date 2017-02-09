#pragma once

#include <chrono>

namespace phosphor
{
namespace time
{

class BmcTimeChangeListener
{
    public:
        virtual ~BmcTimeChangeListener() {}

        /** @brief Notified on bmc time is changed
         *
         * @param[in] bmcTime - The epoch time in microseconds
         */
        virtual void onBmcTimeChanged(std::chrono::microseconds bmcTime) = 0;
};

}
}
