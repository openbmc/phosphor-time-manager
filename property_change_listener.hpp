#pragma once

#include "types.hpp"

namespace phosphor
{
namespace time
{

class PropertyChangeListner
{
    public:
        virtual ~PropertyChangeListner() {}

        /** @brief Notified on time mode is changed */
        virtual void onModeChanged(Mode mode) = 0;

        /** @brief Notified on time owner is changed */
        virtual void onOwnerChanged(Owner owner) = 0;
};

}
}
