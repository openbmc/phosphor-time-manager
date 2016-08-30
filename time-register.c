#include <systemd/sd-bus.h>
#include "time-register.hpp"
#include <sys/timerfd.h>
#include <errno.h>
#include <stdio.h>

/* Function pointer of APIs exposed via Dbus */
//const sd_bus_vtable timeServicesVtable[] =
const sd_bus_vtable timeServicesVtable [] =
{
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("curr_time_mode", "s", getCurrTimeModeProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("curr_time_owner", "s", getCurrTimeOwnerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("requested_time_mode", "s", getReqTimeModeProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("requested_time_owner", "s", getReqTimeOwnerProperty, 0,
    SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("GetTime", "s", "sx", &GetTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetTime", "ss", "i", &SetTime,
    SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};
