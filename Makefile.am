AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = phosphor-timemanager

noinst_LTLIBRARIES = libtimemanager.la

libtimemanager_la_SOURCES = \
	epoch_base.cpp \
	bmc_epoch.cpp \
	host_epoch.cpp \
	manager.cpp

phosphor_timemanager_SOURCES = \
	main.cpp

phosphor_timemanager_LDADD = libtimemanager.la

generic_cxx_flags = $(SYSTEMD_CFLAGS) \
                    $(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
                    $(SDBUSPLUS_CFLAGS)

generic_ld_flags = $(SYSTEMD_LIBS) \
                   $(PHOSPHOR_DBUS_INTERFACES_LIBS) \
                   $(SDBUSPLUS_LIBS)

libtimemanager_la_CXXFLAGS = $(generic_cxx_flags)
libtimemanager_la_LIBADD = $(generic_ld_flags)

phosphor_timemanager_CXXFLAGS = $(generic_cxx_flags)

phosphor_timemanager_LDFLAGS = $(generic_ld_flags)

SUBDIRS = . test
