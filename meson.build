project('phosphor-time-manager',
        'cpp',
         version: '0.1',
         meson_version: '>=0.53.2',
         default_options: [ 'warning_level=3',
                            'werror=true',
                            'cpp_std=c++17' ])

########################################################################

# Project Links

project_pretty_name = 'phosphor-time-manager'
project_url = 'https://github.com/openbmc/' + project_pretty_name
project_issues_url = project_url + '/issues/new'

summary('Issue', project_issues_url, section : 'Report Issues')

#####################################################################

# Validate the c++ Standard

if get_option('cpp_std') != 'c++17'
    error('This project requires cpp std to be in c++17 mode')
endif

#########################################################################

# Get Compiler and default build type

compiler = meson.get_compiler('cpp')

#########################################################################

# Find the dependencies

cppfs           =   compiler.find_library('stdc++fs')
systemd         =   dependency ('systemd', version: '>=221')
sdbusplus       =   dependency ('sdbusplus')
logging         =   dependency ('phosphor-logging')
dbus            =   dependency ('phosphor-dbus-interfaces')
deps            =   [cppfs, systemd, sdbusplus, logging, dbus]


if compiler.has_header('systemd/sd-bus.h')
    ### Nothing to do
else
    warning('Systemd Development Package Required')
endif

###########################################################################

# Set the compiler flags

add_project_arguments(

  compiler.get_supported_arguments([
    '-Wno-psabi',
    '-Wno-pedantic',
    '-Wno-missing-field-initializers',
    '-fpic',
    '-Wno-unused-result',
    ]),
    language : 'cpp')


############################################################################

# Get the config data and enable options

conf_data = configuration_data()

conf_data.set_quoted('BUSNAME', get_option('BUSNAME'))

conf_data.set_quoted('OBJPATH_BMC', get_option('OBJPATH_BMC'))

conf_data.set('DEFAULT_TIME_MODE', get_option('DEFAULT_TIME_MODE'))

configure_file(output: 'config.h', configuration: conf_data)


############################################################################

# Gather sources for the target binaries

phosphor_time_manager_sources = [
                                 'epoch_base.cpp',
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

systemd_system_unit_dir = systemd.get_pkgconfig_variable('systemdsystemunitdir',
                                                         define_variable: ['prefix',
                                                                           get_option('prefix')
                                                                          ])
configure_file(input : 'xyz.openbmc_project.Time.Manager.service.in',
               output : 'xyz.openbmc_project.Time.Manager.service',
               configuration: conf_data,
               install_dir: systemd_system_unit_dir,
               install : true)

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

