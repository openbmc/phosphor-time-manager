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
        virtual void onBmcTimeChanged(std::chrono::microseconds bmcTime) = 0;
};

}
}
