#include "utils.hpp"

#include <xyz/openbmc_project/Common/error.hpp>

#include <gtest/gtest.h>

namespace phosphor
{
namespace time
{
namespace utils
{

using InvalidEnumString = sdbusplus::exception::InvalidEnumString;

TEST(TestUtil, strToMode)
{
    EXPECT_EQ(Mode::NTP,
              strToMode("xyz.openbmc_project.Time.Synchronization.Method.NTP"));
    EXPECT_EQ(
        Mode::Manual,
        strToMode("xyz.openbmc_project.Time.Synchronization.Method.Manual"));

    // All unrecognized strings result in InvalidEnumString exception
    EXPECT_THROW(strToMode(""), InvalidEnumString);
    EXPECT_THROW(
        strToMode("xyz.openbmc_project.Time.Synchronization.Method.MANUAL"),
        InvalidEnumString);
    EXPECT_THROW(strToMode("whatever"), InvalidEnumString);
}

} // namespace utils
} // namespace time
} // namespace phosphor
