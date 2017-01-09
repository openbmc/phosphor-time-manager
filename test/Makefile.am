AM_CPPFLAGS = -I${top_srcdir}
check_PROGRAMS =

TESTS = $(check_PROGRAMS)

check_PROGRAMS += test

test_SOURCES = \
    TestTimeManager.cpp \
    ../time_manager.cpp \
	../xyz.openbmc_project.Time.EpochTime.cpp

test_CPPFLAGS = -Igtest $(GTEST_CPPFLAGS) $(AM_CPPFLAGS)
test_CXXFLAGS = $(PTHREAD_CFLAGS)
test_LDFLAGS = -lgtest_main -lgtest $(PTHREAD_LIBS) $(OESDK_TESTCASE_FLAGS)
test_LDFLAGS += $(SYSTEMD_LIBS)
#test_LDADD = $(top_builddir)/sample.o
