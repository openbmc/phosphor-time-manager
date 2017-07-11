# Initialization
AC_PREREQ([2.69])
AC_INIT([phosphor-time-manager], [1.0], [https://github.com/openbmc/phosphor-time-manager/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs
AC_PROG_CXX
AM_PROG_AR

# Surpress the --with-libtool-sysroot error
LT_INIT

# Check for libraries
PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 221],,\
    AC_MSG_ERROR(["Systemd required and not found"]))
PKG_CHECK_MODULES([PHOSPHOR_DBUS_INTERFACES], [phosphor-dbus-interfaces],,\
    AC_MSG_ERROR(["Requires phosphor-dbus-interfaces package."]))
PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus],,\
    AC_MSG_ERROR(["Requires sdbusplus package."]))
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,\
    AC_MSG_ERROR(["Requires phosphor-logging package."]))

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

# Check for sdbus++ tool
AC_PATH_PROG([SDBUSPLUSPLUS], [sdbus++])
AS_IF([test "x$SDBUSPLUSPLUS" == "x"], AC_MSG_ERROR([Cannot find sdbus++]))

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-fpic -Wall -Werror], [CXXFLAGS])

# DBUS interface
AC_ARG_VAR(BUSNAME, [The Time Manager Dbus busname to own])
AS_IF([test "x$BUSNAME" == "x"], [BUSNAME="xyz.openbmc_project.Time.Manager"])
AC_DEFINE_UNQUOTED([BUSNAME], ["$BUSNAME"], [The Time Manager DBus busname to own])

AC_ARG_VAR(OBJPATH_BMC, [The bmc epoch Dbus root])
AS_IF([test "x$OBJPATH_BMC" == "x"], [OBJPATH_BMC="/xyz/openbmc_project/time/bmc"])
AC_DEFINE_UNQUOTED([OBJPATH_BMC], ["$OBJPATH_BMC"], [The bmc epoch Dbus root])

AC_ARG_VAR(OBJPATH_HOST, [The host epoch Dbus root])
AS_IF([test "x$OBJPATH_HOST" == "x"], [OBJPATH_HOST="/xyz/openbmc_project/time/host"])
AC_DEFINE_UNQUOTED([OBJPATH_HOST], ["$OBJPATH_HOST"], [The host epoch Dbus root])

AC_ARG_VAR(HOST_OFFSET_FILE, [The file to save host time offset])
AS_IF([test "x$HOST_OFFSET_FILE" == "x"], [HOST_OFFSET_FILE="/var/lib/obmc/saved_host_offset"])
AC_DEFINE_UNQUOTED([HOST_OFFSET_FILE], ["$HOST_OFFSET_FILE"], [The file to save host time offset])


AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
