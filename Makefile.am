AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	phosphor-timemanager

phosphor_timemanager_SOURCES = \
	main.cpp \
	epoch_base.cpp \
	bmc_epoch.cpp \
	host_epoch.cpp \
	xyz.openbmc_project.Time.EpochTime.cpp


phosphor_timemanager_LDFLAGS = $(SYSTEMD_LIBS)

SUBDIRS = test
