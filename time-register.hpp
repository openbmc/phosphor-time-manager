#include <systemd/sd-bus.h>
#ifdef __cplusplus
extern "C" {
#endif
int getTimeManagerProperty(sd_bus*, const char*, const char*, const char*,
                           sd_bus_message*, void*, sd_bus_error*);
int GetTime(sd_bus_message*, void*, sd_bus_error*);
int SetTime(sd_bus_message*, void*, sd_bus_error*);
#ifdef __cplusplus
};
#endif
