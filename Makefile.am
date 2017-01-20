AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	timemanager

timemanager_SOURCES = \
	main.cpp \
	epoch_base.cpp \
	bmc_epoch.cpp \
	xyz.openbmc_project.Time.EpochTime.cpp


timemanager_LDFLAGS = $(SYSTEMD_LIBS)

SUBDIRS = test
