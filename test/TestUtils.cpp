#include "utils.hpp"

#include <gtest/gtest.h>
#include <xyz/openbmc_project/Common/error.hpp>

namespace phosphor
{
namespace time
{
namespace utils
{

using InvalidArgument =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;

TEST(TestUtil, strToMode)
{
    EXPECT_EQ(Mode::NTP, strToMode("NTP"));
    EXPECT_EQ(Mode::MANUAL, strToMode("MANUAL"));

    // All unrecognized strings result in InvalidArgument exception
    EXPECT_THROW(strToMode(""), InvalidArgument);
    EXPECT_THROW(strToMode("Manual"), InvalidArgument);
    EXPECT_THROW(strToMode("whatever"), InvalidArgument);
}


TEST(TestUtil, strToOwner)
{
    EXPECT_EQ(Owner::BMC, strToOwner("BMC"));
    EXPECT_EQ(Owner::HOST, strToOwner("HOST"));
    EXPECT_EQ(Owner::SPLIT, strToOwner("SPLIT"));
    EXPECT_EQ(Owner::BOTH, strToOwner("BOTH"));

    // All unrecognized strings result in InvalidArgument exception
    EXPECT_THROW(strToOwner(""), InvalidArgument);
    EXPECT_THROW(strToOwner("Split"), InvalidArgument);
    EXPECT_THROW(strToOwner("xyz"), InvalidArgument);
}

TEST(TestUtil, modeToStr)
{
    EXPECT_EQ("NTP", modeToStr(Mode::NTP));
    EXPECT_EQ("MANUAL", modeToStr(Mode::MANUAL));

    // All unrecognized strings are unknown
    EXPECT_EQ("Unknown", modeToStr(static_cast<Mode>(100)));
}

TEST(TestUtil, ownerToStr)
{
    EXPECT_EQ("BMC", ownerToStr(Owner::BMC));
    EXPECT_EQ("HOST", ownerToStr(Owner::HOST));
    EXPECT_EQ("SPLIT", ownerToStr(Owner::SPLIT));
    EXPECT_EQ("BOTH", ownerToStr(Owner::BOTH));

    // All unrecognized strings are unknown
    EXPECT_EQ("Unknown", ownerToStr(static_cast<Owner>(100)));
}

} // namespace utils
} // namespace time
} // namespace phosphor
