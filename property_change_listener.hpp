#pragma once

#include "types.hpp"

namespace phosphor
{
namespace time
{

class PropertyChangeListner
{
  public:
    virtual ~PropertyChangeListner() = default;

    /** @brief Notified on time mode is changed */
    virtual void onModeChanged(Mode mode) = 0;
};

} // namespace time
} // namespace phosphor
