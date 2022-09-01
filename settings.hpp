#pragma once

#include <sdbusplus/bus.hpp>

#include <string>

namespace settings
{

using Path = std::string;
using Service = std::string;
using Interface = std::string;

constexpr auto root = "/";
constexpr auto timeSyncIntf = "xyz.openbmc_project.Time.Synchronization";

/** @class Objects
 *  @brief Fetch paths of settings D-bus objects of interest upon construction
 */
struct Objects
{
  public:
    /** @brief Constructor - fetch settings objects
     *
     * @param[in] bus - The D-bus bus object
     */
    Objects() = delete;
    explicit Objects(sdbusplus::bus_t&);
    Objects(const Objects&) = delete;
    Objects& operator=(const Objects&) = delete;
    Objects(Objects&&) = default;
    Objects& operator=(Objects&&) = default;
    ~Objects() = default;

    /** @brief time sync method settings object */
    Path timeSyncMethod;

  private:
    sdbusplus::bus_t& bus;
};

} // namespace settings
