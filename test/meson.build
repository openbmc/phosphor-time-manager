#################################################################################
# Enforce the test dependencies when tests are enabled
dl_dep         =        compiler.find_library('dl', required : get_option('tests'))
gtest          =        dependency('gtest', main: true, disabler: true,required : get_option('tests'))
gmock          =        dependency('gmock',required : get_option('tests'))
thread_dep     =        dependency('threads', required : get_option('tests'))


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
                 dependencies : [ gtest,
                                  gmock,
                                  thread_dep,
                                  dl_dep,
                                ] + deps))
endforeach

