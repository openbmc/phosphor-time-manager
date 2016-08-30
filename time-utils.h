#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <unistd.h>
#include <stdio.h>

/* Possible time modes */
typedef enum {
	NTP = 0,
	MANUAL,
}time_mode_t;

/* Possible time owners */
typedef enum {
	BMC = 0,
	HOST,
	SPLIT,
	BOTH,
}time_owner_t;

/* Given a time mode string, returns the equivalent enum */
time_mode_t get_time_mode(const char *time_mode);

/* Accepts time_mode enum and returns it's string equivalent */
const char *mode_str(const time_mode_t time_mode);

/* Given a time owner string, returns the equivalent enum */
time_owner_t get_time_owner(const char *time_owner);

/* Accepts time_owner enum and returns it's string equivalent */
const char *owner_str(const time_owner_t time_owner);

/*
 * Generic file reader used to read time mode, time owner,
 * host time and host offset
 */
int read_data(const char *file_name, void *buffer, size_t len);

/*
 * Generic file writer used to write time mode, time owner,
 * host time and host offset
 */
int write_data(const char *file_name, void *buffer, size_t len);

/*
 * Takes a system setting parameter and returns its value
 * provided the specifier is a string.
 */
char *get_system_settings(const char *user_setting);

/* Reads the PGOOD property from /org/openbmc/control/power0 */
int get_pgood_value();

/* Updates .network file with UseNtp= */
int updt_network_settings(const char *use_dhcp_ntp);

/* Accepts the dbus path and returns it's provider */
char *get_provider(const char *obj_path);
#endif
