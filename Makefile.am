sbin_PROGRAMS = phosphor-time-manager

phosphor_time_manager_SOURCES = \
	xyz.openbmc_project.Time.EpochTime.cpp \
	time-manager.cpp \
	time-manager_main.cpp

# systemd required for journal interfaces
phosphor_time_manager_LDFLAGS = $(SYSTEMD_LIBS)
