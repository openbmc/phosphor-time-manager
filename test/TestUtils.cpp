#include "utils.hpp"

#include <gtest/gtest.h>
#include <xyz/openbmc_project/Common/error.hpp>

namespace phosphor
{
namespace time
{
namespace utils
{

using InvalidEnumString = sdbusplus::exception::InvalidEnumString;

TEST(TestUtil, strToMode)
{
    EXPECT_EQ(
        Mode::NTP,
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


TEST(TestUtil, strToOwner)
{
    EXPECT_EQ(Owner::BMC,
              strToOwner("xyz.openbmc_project.Time.Owner.Owners.BMC"));
    EXPECT_EQ(Owner::Host,
              strToOwner("xyz.openbmc_project.Time.Owner.Owners.Host"));
    EXPECT_EQ(Owner::Split,
              strToOwner("xyz.openbmc_project.Time.Owner.Owners.Split"));
    EXPECT_EQ(Owner::Both,
              strToOwner("xyz.openbmc_project.Time.Owner.Owners.Both"));

    // All unrecognized strings result in InvalidEnumString exception
    EXPECT_THROW(strToOwner(""), InvalidEnumString);
    EXPECT_THROW(strToOwner("Split"), InvalidEnumString);
    EXPECT_THROW(strToOwner("xyz"), InvalidEnumString);
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

TEST(TestUtil, ownerToStr)
{
    EXPECT_EQ("xyz.openbmc_project.Time.Owner.Owners.BMC",
              ownerToStr(Owner::BMC));
    EXPECT_EQ("xyz.openbmc_project.Time.Owner.Owners.Host",
              ownerToStr(Owner::Host));
    EXPECT_EQ("xyz.openbmc_project.Time.Owner.Owners.Split",
              ownerToStr(Owner::Split));
    EXPECT_EQ("xyz.openbmc_project.Time.Owner.Owners.Both",
              ownerToStr(Owner::Both));

    // All unrecognized strings result in exception
    EXPECT_ANY_THROW(ownerToStr(static_cast<Owner>(100)));
}

} // namespace utils
} // namespace time
} // namespace phosphor
