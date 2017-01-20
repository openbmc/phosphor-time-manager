#include <algorithm>
#include <sdbusplus/server.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Time/EpochTime/server.hpp>

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

EpochTime::EpochTime(bus::bus& bus, const char* path)
        : _xyz_openbmc_project_Time_EpochTime_interface(
                bus, path, _interface, _vtable, this)
{
}



auto EpochTime::elapsed() const ->
        uint64_t
{
    return _elapsed;
}

int EpochTime::_callback_get_Elapsed(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* reply, void* context,
        sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

    try
    {
        auto m = message::message(sd_bus_message_ref(reply));

        auto o = static_cast<EpochTime*>(context);
        m.append(convertForMessage(o->elapsed()));
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

auto EpochTime::elapsed(uint64_t value) ->
        uint64_t
{
    if (_elapsed != value)
    {
        _elapsed = value;
        _xyz_openbmc_project_Time_EpochTime_interface.property_changed("Elapsed");
    }

    return _elapsed;
}

int EpochTime::_callback_set_Elapsed(
        sd_bus* bus, const char* path, const char* interface,
        const char* property, sd_bus_message* value, void* context,
        sd_bus_error* error)
{
    try
    {
        auto m = message::message(sd_bus_message_ref(value));

        auto o = static_cast<EpochTime*>(context);

        uint64_t v{};
        m.read(v);
        o->elapsed(v);
    }
    catch(sdbusplus::internal_exception_t& e)
    {
        sd_bus_error_set_const(error, e.name(), e.description());
        return -EINVAL;
    }

    return true;
}

namespace details
{
namespace EpochTime
{
static const auto _property_Elapsed =
    utility::tuple_to_array(message::types::type_id<
            uint64_t>());
}
}


const vtable::vtable_t EpochTime::_vtable[] = {
    vtable::start(),
    vtable::property("Elapsed",
                     details::EpochTime::_property_Elapsed
                        .data(),
                     _callback_get_Elapsed,
                     _callback_set_Elapsed,
                     vtable::property_::emits_change),
    vtable::end()
};

} // namespace server
} // namespace Time
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus

