AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	phosphor-timemanager

phosphor_timemanager_SOURCES = \
	main.cpp \
	epoch_base.cpp \
	bmc_epoch.cpp \
	host_epoch.cpp \
	manager.cpp


phosphor_timemanager_CXXFLAGS = $(SYSTEMD_CFLAGS) $(PHOSPHOR_DBUS_INTERFACES_CFLAGS) $(SDBUSPLUS_CFLAGS)
phosphor_timemanager_LDFLAGS = $(SYSTEMD_LIBS) $(PHOSPHOR_DBUS_INTERFACES_LIBS) $(SDBUSPLUS_LIBS)

SUBDIRS = test
