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
        virtual void onModeChanged(Mode mode) = 0;
        virtual void onOwnerChanged(Owner owner) = 0;
};

}
}
