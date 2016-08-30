#include <systemd/sd-bus.h>
#include "time-manager.hpp"
#include <sys/timerfd.h>
#include <errno.h>
#include <stdio.h>

/* Function pointer of APIs exposed via Dbus */
//const sd_bus_vtable timeServicesVtable[] =
const sd_bus_vtable timeServicesVtable [] =
{
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("curr_time_mode", "s", getTimeManagerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("curr_time_owner", "s", getTimeManagerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("requested_time_mode", "s", getTimeManagerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("requested_time_owner", "s", getTimeManagerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("SetBmcTime", "s", "i", &setBmcTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetHostTime", "x", "i", &setHostTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetBmcTime", "", "xs", &getBmcTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetHostTime", "", "x", &getHostTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};
