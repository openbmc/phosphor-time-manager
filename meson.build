project('phosphor-time-manager',
	    'cpp',
	     version: '0.1',
	     meson_version: '>=0.49.0',
         default_options: [ 'warning_level=3',
			                'werror=true',
	  		                'cpp_std=c++17' ])

add_project_arguments('-Wno-psabi', language: 'cpp')
add_project_link_arguments('-lsystemd',language:'cpp')

conf_data = configuration_data()
conf_data.set_quoted('BUSNAME', 'xyz.openbmc_project.Time.Manager')
conf_data.set_quoted('OBJPATH_BMC','/xyz/openbmc_project/time/bmc')
conf_data.set_quoted('OBJPATH_HOST','/xyz/openbmc_project/time/host')
conf_data.set_quoted('HOST_OFFSET_FILE','/var/lib/obmc/saved_host_offset')
conf_data.set('DEFAULT_TIME_MODE','Mode::Manual')
conf_data.set('DEFAULT_TIME_OWNER','Owner::Both')
configure_file(output: 'config.h',
configuration: conf_data)

deps = [ dependency ('systemd', version: '>=221'),
		 dependency ('sdbusplus'),
		 dependency ('phosphor-logging'),
		 dependency ('phosphor-dbus-interfaces'),
         dependency ('threads')]

compiler = meson.get_compiler('cpp')
compiler.has_header('CLI/CLI.hpp')
python_dep = dependency('python', version: '2.7')
python = find_program('python', required:true)

sdbuspp = find_program('sdbus++')

libmanager_sources = ['epoch_base.cpp',
		      'bmc_epoch.cpp',
		      'host_epoch.cpp',
		      'manager.cpp',
		      'utils.cpp',
		      'settings.cpp']


libtimemanager = static_library('libtimemanager',
 				libmanager_sources,
				dependencies: deps)

executable(
  'phosphor-timemanager',
  'main.cpp',
  link_with: libtimemanager,
  dependencies:deps,
  install: true)
