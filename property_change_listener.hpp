#pragma once

#include "types.hpp"

namespace phosphor
{
namespace time
{

class PropertyChangeListner
{
  public:
    PropertyChangeListner() = default;
    virtual ~PropertyChangeListner() = default;

    PropertyChangeListner(const PropertyChangeListner&) = delete;
    PropertyChangeListner(PropertyChangeListner&&) = delete;
    PropertyChangeListner& operator=(const PropertyChangeListner&) = delete;
    PropertyChangeListner& operator=(PropertyChangeListner&&) = delete;

    /** @brief Notified on time mode is changed */
    virtual void onModeChanged(Mode mode) = 0;
};

} // namespace time
} // namespace phosphor
