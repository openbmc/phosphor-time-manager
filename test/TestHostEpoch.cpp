#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "host_epoch.hpp"
#include "utils.hpp"
#include "config.h"
#include "types.hpp"

namespace phosphor
{
namespace time
{

using namespace std::chrono;
using namespace std::chrono_literals;

class TestHostEpoch : public testing::Test
{
    public:
        sdbusplus::bus::bus bus;
        HostEpoch hostEpoch;

        static constexpr auto FILE_NOT_EXIST = "path/to/file-not-exist";
        static constexpr auto FILE_OFFSET = "saved_host_offset";
        const microseconds delta = 2s;

        TestHostEpoch()
            : bus(sdbusplus::bus::new_default()),
              hostEpoch(bus, OBJPATH_HOST)
        {
            // Make sure the file does not exist
            std::remove(FILE_NOT_EXIST);
        }
        ~TestHostEpoch()
        {
            // Cleanup test file
            std::remove(FILE_OFFSET);
        }

        // Proxies for HostEpoch's private members and functions
        Mode getTimeMode()
        {
            return hostEpoch.timeMode;
        }
        Owner getTimeOwner()
        {
            return hostEpoch.timeOwner;
        }
        microseconds getOffset()
        {
            return hostEpoch.offset;
        }
        void setOffset(microseconds us)
        {
            hostEpoch.offset = us;
        }
        void setTimeOwner(Owner owner)
        {
            hostEpoch.timeOwner = owner;
        }
        void setTimeMode(Mode mode)
        {
            hostEpoch.timeMode = mode;
        }

        void checkSettingTimeNotAllowed()
        {
            // By default offset shall be 0
            EXPECT_EQ(0, getOffset().count());

            // Set time is not allowed,
            // so verify offset is still 0 after set time
            microseconds diff = 1min;
            hostEpoch.elapsed(hostEpoch.elapsed() + diff.count());
            EXPECT_EQ(0, getOffset().count());
            // TODO: when gmock is ready, check there is no call to timedatectl
        }

        void checkSetSplitTimeInFuture()
        {
            // Get current time, and set future +1min time
            auto t1 = hostEpoch.elapsed();
            EXPECT_NE(0, t1);
            microseconds diff = 1min;
            auto t2 = t1 + diff.count();
            hostEpoch.elapsed(t2);

            // Verify that the offset shall be positive,
            // and less or equal to diff, and shall be not too less.
            auto offset = getOffset();
            EXPECT_GT(offset, microseconds(0));
            EXPECT_LE(offset, diff);
            diff -= delta;
            EXPECT_GE(offset, diff);

            // Now get time shall be around future +1min time
            auto epochNow = duration_cast<microseconds>(
                                system_clock::now().time_since_epoch()).count();
            auto elapsedGot = hostEpoch.elapsed();
            EXPECT_LT(epochNow, elapsedGot);
            auto epochDiff = elapsedGot - epochNow;
            diff = 1min;
            EXPECT_GT(epochDiff, (diff - delta).count());
            EXPECT_LT(epochDiff, (diff + delta).count());
        }
        void checkSetSplitTimeInPast()
        {
            // Get current time, and set past -1min time
            auto t1 = hostEpoch.elapsed();
            EXPECT_NE(0, t1);
            microseconds diff = 1min;
            auto t2 = t1 - diff.count();
            hostEpoch.elapsed(t2);

            // Verify that the offset shall be negative, and the absolute value
            // shall be equal or greater than diff, and shall not be too greater
            auto offset = getOffset();
            EXPECT_LT(offset, microseconds(0));
            offset = -offset;
            EXPECT_GE(offset, diff);
            diff += 10s;
            EXPECT_LE(offset, diff);

            // Now get time shall be around past -1min time
            auto epochNow = duration_cast<microseconds>(
                                system_clock::now().time_since_epoch()).count();
            auto elapsedGot = hostEpoch.elapsed();
            EXPECT_LT(elapsedGot, epochNow);
            auto epochDiff = epochNow - elapsedGot;
            diff = 1min;
            EXPECT_GT(epochDiff, (diff - delta).count());
            EXPECT_LT(epochDiff, (diff + delta).count());
        }
};

TEST_F(TestHostEpoch, empty)
{
    EXPECT_EQ(Mode::NTP, getTimeMode());
    EXPECT_EQ(Owner::BMC, getTimeOwner());
}

TEST_F(TestHostEpoch, readDataFileNotExist)
{
    // When file does not exist, the default offset shall be 0
    microseconds offset(0);
    auto value = utils::readData<decltype(offset)::rep>(FILE_NOT_EXIST);
    EXPECT_EQ(0, value);
}

TEST_F(TestHostEpoch, writeAndReadData)
{
    // Write offset to file
    microseconds offsetToWrite(1234567);
    utils::writeData<decltype(offsetToWrite)::rep>(
        FILE_OFFSET, offsetToWrite.count());

    // Read it back
    microseconds offsetToRead;
    offsetToRead = microseconds(
                       utils::readData<decltype(offsetToRead)::rep>(FILE_OFFSET));
    EXPECT_EQ(offsetToWrite, offsetToRead);
}

TEST_F(TestHostEpoch, setElapsedInNtpBmc)
{
    // Set time in NTP/BMC is not allowed
    setTimeMode(Mode::NTP);
    setTimeOwner(Owner::BMC);
    checkSettingTimeNotAllowed();
}

TEST_F(TestHostEpoch, setElapsedInNtpHost)
{
    // Set time in NTP/HOST is not valid
    setTimeMode(Mode::NTP);
    setTimeOwner(Owner::HOST);
    checkSettingTimeNotAllowed();
}

TEST_F(TestHostEpoch, setElapsedInNtpSplit)
{
    // Set time in NTP/SPLIT, offset will be set
    setTimeMode(Mode::NTP);
    setTimeOwner(Owner::SPLIT);

    checkSetSplitTimeInFuture();

    // Reset offset
    setOffset(microseconds(0));
    checkSetSplitTimeInPast();
}

TEST_F(TestHostEpoch, setElapsedInNtpBoth)
{
    // Set time in NTP/BOTH, time will be set to BMC
    // However it requies gmock to test this case
    // TODO: when gmock is ready, test this case.
    setTimeMode(Mode::NTP);
    setTimeOwner(Owner::BOTH);
}

TEST_F(TestHostEpoch, setElapsedInManualBmc)
{
    // Set time in MANUAL/BMC is not allowed
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::BMC);
    checkSettingTimeNotAllowed();
}

TEST_F(TestHostEpoch, setElapsedInManualHost)
{
    // Set time in MANUAL/HOST, time will be set to BMC
    // However it requies gmock to test this case
    // TODO: when gmock is ready, test this case.
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::HOST);
}

TEST_F(TestHostEpoch, setElapsedInManualSplit)
{
    // Set to SPLIT owner so that offset will be set
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::SPLIT);

    checkSetSplitTimeInFuture();

    // Reset offset
    setOffset(microseconds(0));
    checkSetSplitTimeInPast();
}

TEST_F(TestHostEpoch, setElapsedInManualBoth)
{
    // Set time in MANUAL/BOTH, time will be set to BMC
    // However it requies gmock to test this case
    // TODO: when gmock is ready, test this case.
    setTimeMode(Mode::MANUAL);
    setTimeOwner(Owner::BOTH);
}

TEST_F(TestHostEpoch, setElapsedInSplitAndBmcTimeIsChanged)
{
    // Set to SPLIT owner so that offset will be set
    setTimeOwner(Owner::SPLIT);

    // Get current time, and set future +1min time
    auto t1 = hostEpoch.elapsed();
    EXPECT_NE(0, t1);
    microseconds diff = 1min;
    auto t2 = t1 + diff.count();
    hostEpoch.elapsed(t2);

    // Verify that the offset shall be positive,
    // and less or equal to diff, and shall be not too less.
    auto offset = getOffset();
    EXPECT_GT(offset, microseconds(0));
    EXPECT_LE(offset, diff);
    diff -= delta;
    EXPECT_GE(offset, diff);

    // Now BMC time is changed to future +1min
    hostEpoch.onBmcTimeChanged(microseconds(t2));

    // Verify that the offset shall be around zero since it's almost
    // the same as BMC time
    offset = getOffset();
    if (offset.count() < 0)
    {
        offset = microseconds(-offset.count());
    }
    EXPECT_LE(offset, delta);
}

}
}
