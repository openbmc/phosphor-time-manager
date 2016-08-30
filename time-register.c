#include <systemd/sd-bus.h>

/* All defined in time-manager.cpp */
extern int getTimeManagerProperty();
extern int setBmcTime();
extern int setHostTime();
extern int getBmcTime();
extern int getHostTime();

/* Function pointer of APIs exposed via Dbus */
const sd_bus_vtable timeServicesVtable[] =
{
	SD_BUS_VTABLE_START(0),
	SD_BUS_PROPERTY("curr_time_mode", "s", getTimeManagerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("curr_time_owner", "s", getTimeManagerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("requested_time_mode", "s", getTimeManagerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_PROPERTY("requested_time_owner", "s", getTimeManagerProperty, 0, SD_BUS_VTABLE_PROPERTY_CONST),
	SD_BUS_METHOD("SetBMCTime", "s", "is", &setBmcTime, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetHostTime", "x", "is", &setHostTime, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetBMCTime", "", "xs", &getBmcTime, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetHostTime", "", "x", &getHostTime, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END,
};
