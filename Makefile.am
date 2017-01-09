AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	timemanager

timemanager_SOURCES = \
	main.cpp \
	time_manager.cpp \
	xyz.openbmc_project.Time.EpochTime.cpp


timemanager_LDFLAGS = $(SYSTEMD_LIBS)

SUBDIRS = test
