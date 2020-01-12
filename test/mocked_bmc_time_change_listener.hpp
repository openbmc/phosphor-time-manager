#pragma once
#include "bmc_time_change_listener.hpp"

#include <gmock/gmock.h>

namespace phosphor
{
namespace time
{

class MockBmcTimeChangeListener : public BmcTimeChangeListener
{
  public:
    MOCK_METHOD1(onBmcTimeChanged, void(const std::chrono::microseconds&));
};

} // namespace time
} // namespace phosphor
