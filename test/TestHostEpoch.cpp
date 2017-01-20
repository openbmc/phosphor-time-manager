#include <sdbusplus/bus.hpp>
#include <gtest/gtest.h>

#include "host_epoch.hpp"
#include "utils.hpp"
#include "config.h"

namespace phosphor
{
namespace time
{

using namespace std::chrono;
using namespace std::chrono_literals;

class TestHostEpoch : public testing::Test
{
    public:
        using Mode = EpochBase::Mode;
        using Owner = EpochBase::Owner;

        sdbusplus::bus::bus bus;
        Manager manager;
        HostEpoch hostEpoch;

        static constexpr auto FILE_NOT_EXIST = "path/to/file-not-exist";
        static constexpr auto FILE_OFFSET = "saved_host_offset";
        static constexpr auto delta = 2s;

        TestHostEpoch()
            : bus(sdbusplus::bus::new_default()),
              manager(bus),
              hostEpoch(bus, OBJPATH_HOST, &manager)
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
        void setTimeOwner(Owner owner)
        {
            hostEpoch.timeOwner = owner;
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

TEST_F(TestHostEpoch, setElapsedNotAllowed)
{
    // By default offset shall be 0
    EXPECT_EQ(0, getOffset().count());

    // Set time in BMC mode is not allowed,
    // so verify offset is still 0 after set time
    microseconds diff = 1min;
    hostEpoch.elapsed(hostEpoch.elapsed() + diff.count());
    EXPECT_EQ(0, getOffset().count());
}

TEST_F(TestHostEpoch, setElapsedInFutureAndGet)
{
    // Set to HOST owner so that we can set elapsed
    setTimeOwner(Owner::HOST);

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

TEST_F(TestHostEpoch, setElapsedInPastAndGet)
{
    // Set to HOST owner so that we can set elapsed
    setTimeOwner(Owner::HOST);

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

}
}
