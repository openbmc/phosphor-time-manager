project('phosphor-time-manager',
        'cpp',
         version: '0.1',
         meson_version: '>=1.1.1',
         default_options: [ 'warning_level=3',
                            'werror=true',
                            'cpp_std=c++23',
                            'buildtype=debugoptimized' ])

########################################################################

# Project Links

project_pretty_name = 'phosphor-time-manager'
project_url = 'https://github.com/openbmc/' + project_pretty_name
project_issues_url = project_url + '/issues/new'

summary('Issue', project_issues_url, section : 'Report Issues')

#####################################################################

# Get Compiler and default build type

compiler = meson.get_compiler('cpp')

#########################################################################

# Find the dependencies

sdbusplus_dep = dependency ('sdbusplus')
phosphor_logging_dep = dependency ('phosphor-logging')
phosphor_dbus_interfaces_dep = dependency ('phosphor-dbus-interfaces')
deps = [
    sdbusplus_dep,
    phosphor_logging_dep,
    phosphor_dbus_interfaces_dep,
]

###########################################################################

# Get the config data and enable options

conf_data = configuration_data()
conf_data.set('DEFAULT_TIME_MODE', get_option('default_time_mode'))

configure_file(output: 'config.h', configuration: conf_data)


############################################################################

# Gather sources for the target binaries

phosphor_time_manager_sources = [
                                 'bmc_epoch.cpp',
                                 'manager.cpp',
                                 'utils.cpp',
                                 'settings.cpp',
                                 ]

libtimemanager = static_library('libtimemanager',
                                 phosphor_time_manager_sources,
                                 dependencies : deps)
############################################################################

# Install the files into the build directory

systemd = dependency ('systemd')
systemd_system_unit_dir = systemd.get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')])
configure_file(input : 'xyz.openbmc_project.Time.Manager.service',
               output : 'xyz.openbmc_project.Time.Manager.service',
               copy : true,
               install_dir : systemd_system_unit_dir)

#############################################################################

# Build binaries

executable('phosphor-time-manager',
           'main.cpp',
           link_with : libtimemanager,
           dependencies : deps,
           install : true)

if get_option('tests').enabled()
    subdir('test')
endif
