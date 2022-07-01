#include "utils.hpp"

namespace phosphor
{
namespace time
{

namespace // anonymous
{
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
} // namespace

namespace utils
{

std::string getService(sdbusplus::bus::bus& bus, const char* path,
                       const char* interface)
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

    mapper.append(path, std::vector<std::string>({interface}));
    try
    {
        auto mapperResponseMsg = bus.call(mapper);

        std::vector<std::pair<std::string, std::vector<std::string>>>
            mapperResponse;
        mapperResponseMsg.read(mapperResponse);
        if (mapperResponse.empty())
        {
            lg2::error("Error reading mapper response");
            throw std::runtime_error("Error reading mapper response");
        }

        return mapperResponse[0].first;
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        lg2::error("Mapper call failed: path:{PATH}, interface:{INTF}, "
                   "propertyName:{NAME}, Erro:{ERROR}",
                   "PATH", path, "INTF", interface, "ERROR", ex);
        throw std::runtime_error("Mapper call failed");
    }
}

Mode strToMode(const std::string& mode)
{
    return ModeSetting::convertMethodFromString(mode);
}

std::string modeToStr(Mode mode)
{
    return sdbusplus::xyz::openbmc_project::Time::server::convertForMessage(
        mode);
}

} // namespace utils
} // namespace time
} // namespace phosphor
