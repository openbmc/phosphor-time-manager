sbin_PROGRAMS = timemanager

timemanager_SOURCES = \
	xyz.openbmc_project.Time.EpochTime.cpp \
	time-manager.cpp \
	time-manager_main.cpp

# systemd required for journal interfaces
timemanager_LDFLAGS = $(SYSTEMD_LIBS)
