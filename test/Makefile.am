AM_CPPFLAGS = -I${top_srcdir}
check_PROGRAMS =

TESTS = $(check_PROGRAMS)

check_PROGRAMS += test

test_SOURCES = \
    TestEpochBase.cpp \
    TestBmcEpoch.cpp \
    TestHostEpoch.cpp \
	TestManager.cpp \
    ../epoch_base.cpp \
    ../bmc_epoch.cpp \
    ../host_epoch.cpp \
    ../manager.cpp

test_CPPFLAGS = $(GTEST_CPPFLAGS) \
                $(AM_CPPFLAGS)

test_CXXFLAGS = $(PTHREAD_CFLAGS) \
                $(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
                $(SDBUSPLUS_CFLAGS)

test_LDFLAGS = -lgmock_main \
               -lgmock \
               $(PTHREAD_LIBS) \
               $(OESDK_TESTCASE_FLAGS)

test_LDFLAGS += $(PHOSPHOR_DBUS_INTERFACES_LIBS) \
                $(SDBUSPLUS_LIBS)
