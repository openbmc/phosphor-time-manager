#pragma once
#include <tuple>
#include <systemd/sd-bus.h>
#include <sdbusplus/server.hpp>

namespace sdbusplus
{
namespace xyz
{
namespace openbmc_project
{
namespace Time
{
namespace server
{

class EpochTime
{
    public:
        /* Define all of the basic class operations:
         *     Not allowed:
         *         - Default constructor to avoid nullptrs.
         *         - Copy operations due to internal unique_ptr.
         *         - Move operations due to 'this' being registered as the
         *           'context' with sdbus.
         *     Allowed:
         *         - Destructor.
         */
        EpochTime() = delete;
        EpochTime(const EpochTime&) = delete;
        EpochTime& operator=(const EpochTime&) = delete;
        EpochTime(EpochTime&&) = delete;
        EpochTime& operator=(EpochTime&&) = delete;
        virtual ~EpochTime() = default;

        /** @brief Constructor to put object onto bus at a dbus path.
         *  @param[in] bus - Bus to attach to.
         *  @param[in] path - Path to attach at.
         */
        EpochTime(bus::bus& bus, const char* path);




        /** Get value of Elapsed */
        virtual uint64_t elapsed() const;
        /** Set value of Elapsed */
        virtual uint64_t elapsed(uint64_t value);


    private:

        /** @brief sd-bus callback for get-property 'Elapsed' */
        static int _callback_get_Elapsed(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);
        /** @brief sd-bus callback for set-property 'Elapsed' */
        static int _callback_set_Elapsed(
            sd_bus*, const char*, const char*, const char*,
            sd_bus_message*, void*, sd_bus_error*);


        static constexpr auto _interface = "xyz.openbmc_project.Time.EpochTime";
        static const vtable::vtable_t _vtable[];
        sdbusplus::server::interface::interface
                _xyz_openbmc_project_Time_EpochTime_interface;

        uint64_t _elapsed{};

};


} // namespace server
} // namespace Time
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus

