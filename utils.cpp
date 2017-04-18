#include "utils.hpp"

#include <phosphor-logging/log.hpp>


namespace phosphor
{
namespace time
{

namespace // anonymous
{
/** @brief The map that maps the string to Mode */
const std::map<std::string, Mode> modeMap =
{
    { "NTP", Mode::NTP },
    { "MANUAL", Mode::MANUAL },
};

/** @brief The map that maps the string to Owner */
const std::map<std::string, Owner> ownerMap =
{
    { "BMC", Owner::BMC },
    { "HOST", Owner::HOST },
    { "SPLIT", Owner::SPLIT },
    { "BOTH", Owner::BOTH },
};
}

namespace utils
{

using namespace phosphor::logging;

Mode strToMode(const std::string& mode)
{
    auto it = modeMap.find(mode);
    if (it == modeMap.end())
    {
        log<level::ERR>("Unrecognized mode",
                        entry("%s", mode.c_str()));
        return Mode::NTP;
    }
    return it->second;
}

Owner strToOwner(const std::string& owner)
{
    auto it = ownerMap.find(owner);
    if (it == ownerMap.end())
    {
        log<level::ERR>("Unrecognized owner",
                        entry("%s", owner.c_str()));
        return Owner::BMC;
    }
    return it->second;
}

const char* modeToStr(Mode mode)
{
    const char* ret;
    switch (mode)
    {
    case Mode::NTP:
        ret = "NTP";
        break;
    case Mode::MANUAL:
        ret = "MANUAL";
        break;
    default:
        ret = "Unknown";
        break;
    }
    return ret;
}

const char* ownerToStr(Owner owner)
{
    const char* ret;
    switch (owner)
    {
    case Owner::BMC:
        ret = "BMC";
        break;
    case Owner::HOST:
        ret = "HOST";
        break;
    case Owner::SPLIT:
        ret = "SPLIT";
        break;
    case Owner::BOTH:
        ret = "BOTH";
        break;
    default:
        ret = "Unknown";
        break;
    }
    return ret;
}

} // namespace utils
} // namespace time
} // namespace phosphor
