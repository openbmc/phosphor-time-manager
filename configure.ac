# Initialization
AC_PREREQ([2.69])
AC_INIT([phosphor-time-manager], [1.0], [https://github.com/openbmc/phosphor-time-manager/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs
AC_PROG_CXX
# Checks/sets the install variable to be used
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Surpress the --with-libtool-sysroot error
LT_INIT

# Check for libraries
PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 221])

# gtest
# Check/set gtest specific functions.
AX_PTHREAD([GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"],[GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"])
AC_SUBST(GTEST_CPPFLAGS)

# Test cases require SDK so only build if we're told to (and SDK is available)
AC_ARG_ENABLE([oe-sdk],
    AS_HELP_STRING([--enable-oe-sdk], [Link testcases absolutely against OE SDK so they can be ran within it.])
)
AC_ARG_VAR(OECORE_TARGET_SYSROOT,
    [Path to the OE SDK SYSROOT])
AS_IF([test "x$enable_oe_sdk" == "xyes"],
    AS_IF([test "x$OECORE_TARGET_SYSROOT" == "x"],
          AC_MSG_ERROR([OECORE_TARGET_SYSROOT must be set with --enable-oe-sdk])
    )
    AC_MSG_NOTICE([Enabling OE-SDK at $OECORE_TARGET_SYSROOT])
    [
        testcase_flags="-Wl,-rpath,\${OECORE_TARGET_SYSROOT}/lib"
        testcase_flags="${testcase_flags} -Wl,-rpath,\${OECORE_TARGET_SYSROOT}/usr/lib"
        testcase_flags="${testcase_flags} -Wl,-dynamic-linker,`find \${OECORE_TARGET_SYSROOT}/lib/ld-*.so | sort -r -n | head -n1`"
    ]
    AC_SUBST([OESDK_TESTCASE_FLAGS], [$testcase_flags])
)

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-fpic -Wall -Werror], [CXXFLAGS])

# DBUS interface
AC_ARG_VAR(BUSNAME, [The Time Manager Dbus busname to own])
AS_IF([test "x$BUSNAME" == "x"], [BUSNAME="xyz.openbmc_project.Time.Manager"])
AC_DEFINE_UNQUOTED([BUSNAME], ["$BUSNAME"], [The Time Manager DBus busname to own])

AC_ARG_VAR(OBJPATH_BMC, [The bmc epoch Dbus root])
AS_IF([test "x$OBJPATH_BMC" == "x"], [OBJPATH_BMC="/xyz/openbmc_project/Time/Bmc"])
AC_DEFINE_UNQUOTED([OBJPATH_BMC], ["$OBJPATH_BMC"], [The bmc epoch Dbus root])

AC_ARG_VAR(OBJPATH_HOST, [The host epoch Dbus root])
AS_IF([test "x$OBJPATH_HOST" == "x"], [OBJPATH_HOST="/xyz/openbmc_project/Time/Host"])
AC_DEFINE_UNQUOTED([OBJPATH_HOST], ["$OBJPATH_HOST"], [The host epoch Dbus root])

# Check for header files.
AC_CHECK_HEADER(systemd/sd-bus.h, ,[AC_MSG_ERROR([Could not find systemd/sd-bus.h...systemd developement package required])])
AC_CHECK_HEADER(sdbusplus/server.hpp, ,[AC_MSG_ERROR([Could not find sdbusplus/server.hpp...openbmc/sdbusplus package required])])
AC_CHECK_HEADER(log.hpp, ,[AC_MSG_ERROR([Could not find log.hpp...openbmc/phosphor-logging package required])])


AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
