AM_DEFAULT_SOURCE_EXT = .cpp
if !INSTALL_ERROR_YAML
sbin_PROGRAMS = phosphor-timemanager

noinst_LTLIBRARIES = libtimemanager.la

generated_source = xyz/openbmc_project/Time/Internal/error.cpp

BUILT_SOURCES = ${generated_source} \
				xyz/openbmc_project/Time/Internal/error.hpp

CLEANFILES = ${BUILT_SOURCES}

libtimemanager_la_SOURCES = \
	epoch_base.cpp \
	bmc_epoch.cpp \
	host_epoch.cpp \
	manager.cpp \
	utils.cpp \
	settings.cpp \
	${generated_source}

phosphor_timemanager_SOURCES = \
	main.cpp

phosphor_timemanager_LDADD = libtimemanager.la

generic_cxx_flags = $(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
                    $(SDBUSPLUS_CFLAGS)

generic_ld_flags = $(PHOSPHOR_DBUS_INTERFACES_LIBS) \
                   $(SDBUSPLUS_LIBS)

libtimemanager_la_CXXFLAGS = $(generic_cxx_flags)
libtimemanager_la_LIBADD = $(generic_ld_flags)

phosphor_timemanager_CXXFLAGS = $(generic_cxx_flags)

phosphor_timemanager_LDFLAGS = $(generic_ld_flags)

xyz/openbmc_project/Time/Internal/error.hpp: ${top_srcdir}/xyz/openbmc_project/Time/Internal.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(top_srcdir) error exception-header xyz.openbmc_project.Time.Internal > $@

xyz/openbmc_project/Time/Internal/error.cpp: ${top_srcdir}/xyz/openbmc_project/Time/Internal.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(top_srcdir) error exception-cpp xyz.openbmc_project.Time.Internal> $@

SUBDIRS = . test
endif
# Export error YAML to shared location
yamldir = ${datadir}/phosphor-dbus-yaml/yaml
nobase_yaml_DATA = \
	xyz/openbmc_project/Time/Internal.errors.yaml \
	xyz/openbmc_project/Time/Internal.metadata.yaml
