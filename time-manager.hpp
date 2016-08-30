#include <systemd/sd-bus.h>
#ifdef __cplusplus
extern "C" {
#endif
int getTimeManagerProperty(sd_bus*, const char*, const char*, const char*,
                           sd_bus_message*, void*, sd_bus_error*);
int setBmcTime(sd_bus_message*, void*, sd_bus_error*);
int setHostTime(sd_bus_message*, void*, sd_bus_error*);
int getBmcTime(sd_bus_message*, void*, sd_bus_error*);
int getHostTime(sd_bus_message*, void*, sd_bus_error*);
#ifdef __cplusplus
};
#endif
