AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	timemanager

timemanager_SOURCES = \
	time-register.c \
	time-utils.cpp \
    time-manager.cpp

timemanager_LDFLAGS = $(SYSTEMD_LIBS) $(libmapper_LIBS)
