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

TEST(TestUtil, modeToStr)
{
    EXPECT_EQ("xyz.openbmc_project.Time.Synchronization.Method.NTP",
              modeToStr(Mode::NTP));
    EXPECT_EQ("xyz.openbmc_project.Time.Synchronization.Method.Manual",
              modeToStr(Mode::Manual));

    // All unrecognized strings result in exception
    EXPECT_ANY_THROW(modeToStr(static_cast<Mode>(100)));
}

TEST(TestUtil, parseToEpoch)
{
    std::vector<uint8_t> respData = {0, 57, 0,  2, 0,   11, 21,  160, 0,
                                     1, 44, 72, 0, 251, 72, 166, 96};

    // valid response vector will return non zero epoctime
    EXPECT_NE(0, parseToEpoch(respData));
}

} // namespace utils
} // namespace time
} // namespace phosphor
