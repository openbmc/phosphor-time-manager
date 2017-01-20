#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/match.hpp>

#include <set>
#include <string>

namespace phosphor
{
namespace time
{

class PropertyChangeListner
{
    public:
        virtual ~PropertyChangeListner() {}
        virtual void onPropertyChange(const std::string& key,
                                      const std::string& value) = 0;
};

class Manager
{
    public:
        friend class TestTimeManager;
        Manager(sdbusplus::bus::bus& bus);
        void addListener(PropertyChangeListner* listener);
    private:
        sdbusplus::bus::bus& bus;
        sdbusplus::server::match::match propertyChangeMatch;
        sdbusplus::server::match::match pgoodChangeMatch;
        std::set<PropertyChangeListner*> listeners;
        bool isHostOn;
        std::string requestedMode;
        std::string requestedOwner;

        void initPgood();
        void onPropertyChanged(const std::string& key,
                               const std::string& value);
        void onPgoodChanged(bool pgood);
        void saveProperty(const std::string& key,
                          const std::string& value);
        void setRequestedMode(const std::string& mode);
        void setRequestedOwner(const std::string& owner);

        static int onPropertyChanged(sd_bus_message* msg,
                                     void* userData,
                                     sd_bus_error* retError);
        static int onPgoodChanged(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retError);
        static const std::set<std::string> managedProperties;

        static constexpr auto modeFile = "/var/lib/obmc/saved_time_mode";
        static constexpr auto ownerFile = "/var/lib/obmc/saved_time_owner";
};

}
}
