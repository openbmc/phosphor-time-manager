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

        /** @brief Notified on bmc time is changed */
        virtual void onBmcTimeChanged(std::chrono::microseconds bmcTime) = 0;
};

}
}
