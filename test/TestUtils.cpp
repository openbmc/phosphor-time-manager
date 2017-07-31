#include "utils.hpp"

#include <gtest/gtest.h>

namespace phosphor
{
namespace time
{
namespace utils
{

TEST(TestUtil, strToMode)
{
    EXPECT_EQ(Mode::NTP, strToMode("NTP"));
    EXPECT_EQ(Mode::MANUAL, strToMode("MANUAL"));

    // All unrecognized strings result in assertion
    // TODO: use EXPECT_THROW once the code uses elog
    EXPECT_DEATH(strToMode(""), "");
    EXPECT_DEATH(strToMode("Manual"), "");
    EXPECT_DEATH(strToMode("whatever"), "");
}


TEST(TestUtil, strToOwner)
{
    EXPECT_EQ(Owner::BMC, strToOwner("BMC"));
    EXPECT_EQ(Owner::HOST, strToOwner("HOST"));
    EXPECT_EQ(Owner::SPLIT, strToOwner("SPLIT"));
    EXPECT_EQ(Owner::BOTH, strToOwner("BOTH"));

    // All unrecognized strings result in assertion
    // TODO: use EXPECT_THROW once the code uses elog
    EXPECT_DEATH(strToOwner(""), "");
    EXPECT_DEATH(strToOwner("Split"), "");
    EXPECT_DEATH(strToOwner("xyz"), "");
}

TEST(TestUtil, modeToStr)
{
    EXPECT_EQ("NTP", modeToStr(Mode::NTP));
    EXPECT_EQ("MANUAL", modeToStr(Mode::MANUAL));

    // All unrecognized enums result in assertion
    // TODO: use EXPECT_THROW once the code uses elog
    EXPECT_DEATH(modeToStr(static_cast<Mode>(100)), "");
}

TEST(TestUtil, ownerToStr)
{
    EXPECT_EQ("BMC", ownerToStr(Owner::BMC));
    EXPECT_EQ("HOST", ownerToStr(Owner::HOST));
    EXPECT_EQ("SPLIT", ownerToStr(Owner::SPLIT));
    EXPECT_EQ("BOTH", ownerToStr(Owner::BOTH));

    // All unrecognized enums result in assertion
    // TODO: use EXPECT_THROW once the code uses elog
    EXPECT_DEATH(ownerToStr(static_cast<Owner>(100)), "");
}

} // namespace utils
} // namespace time
} // namespace phosphor
