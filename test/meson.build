# Enforce the test dependencies when tests are enabled
dl_dep         =        compiler.find_library('dl', required : get_option('tests'))
gtest          =        dependency('gtest', main: true, disabler: true,required : get_option('tests'))
gmock          =        dependency('gmock',required : get_option('tests'))
thread_dep     =        dependency('threads', required : get_option('tests'))


#################################################################################
# Get the rpath, when executing in an x86 sdk
if get_option('oe-sdk').enabled()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
    error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = ['-Wl,-dynamic-linker,' + ld_so]
else
  dynamic_linker = []
endif
##################################################################################
##################################################################################
# declare the test sources
test_list = [
  'TestBmcEpoch.cpp',
  'TestManager.cpp',
  'TestUtils.cpp',
  'mocked_property_change_listener.hpp',
]
###################################################################################
# Run the tests
foreach tests:test_list
  test_name = tests.split('.')[0]
  test(test_name,
       executable(test_name,tests,
                 include_directories : ['.','../'],
                 link_with : libtimemanager,
                 link_args: dynamic_linker,
                 build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
                 dependencies : [ gtest,
                                  gmock,
                                  thread_dep,
                                  dl_dep,
                                ] + deps), timeout:40)
endforeach

