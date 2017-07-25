AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	timemanager

timemanager_SOURCES = \
	time-register.c \
	time-config.cpp \
	time-manager.cpp \
	settings.cpp

timemanager_LDFLAGS = \
	$(SYSTEMD_LIBS) \
	$(libmapper_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS)
