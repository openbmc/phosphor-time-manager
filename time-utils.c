#include <string.h>
#include <systemd/sd-bus.h>
#include "time-utils.h"

/* Defined in time-manager.c */
extern sd_bus *g_time_bus;
extern time_t g_curr_time_mode;
extern time_t g_curr_time_owner;

/* Given a mode string, returns it's equivalent mode enum */
time_mode_t get_time_mode(const char *time_mode)
{
	time_mode_t new_time_mode = g_curr_time_mode;

	if (!strcasecmp(time_mode, "NTP"))
		new_time_mode = NTP;
	else if (!strcasecmp(time_mode, "Manual"))
		new_time_mode = MANUAL;

	return new_time_mode;
}

/* Accepts a time_mode enum and returns it's string value */
const char *mode_str(const time_mode_t time_mode)
{
	const char *mode_str = NULL;

	switch(time_mode) {
	case NTP:
		mode_str = "NTP";
		break;
	case MANUAL:
		mode_str = "Manual";
		break;
	default:
		mode_str = "INVALID";
		break;
	}
	return mode_str;
}

/* Given a owner string, returns it's equivalent owner enum */
time_owner_t get_time_owner(const char *time_owner)
{
	time_owner_t new_time_owner = g_curr_time_owner;

	if (!strcasecmp(time_owner, "BMC"))
		new_time_owner = BMC;
	else if (!strcasecmp(time_owner, "Host"))
		new_time_owner = HOST;
	else if (!strcasecmp(time_owner, "Split"))
		new_time_owner = SPLIT;
	else if (!strcasecmp(time_owner, "Both"))
		new_time_owner = BOTH;

	return new_time_owner;
}

/* Accepts a time_owner enum and returns it's string value */
const char *owner_str(const time_owner_t time_owner)
{
	const char *owner_str = NULL;

	switch(time_owner) {
	case BMC:
		owner_str = "BMC";
		break;
	case HOST:
		owner_str = "HOST";
		break;
	case SPLIT:
		owner_str = "SPLIT";
		break;
	case BOTH:
		owner_str = "BOTH";
		break;
	default:
		owner_str = "INVALID";
		break;
	}
	return owner_str;
}

/* Generic file reader */
int read_data(const char *file_name, void *buffer, size_t len)
{
	FILE *fp = NULL;
	int data = 0;

	/*
	 * When we first start the daemon, its expected to
	 * get a failure.. So create a file
	 */
	if (access(file_name, F_OK ) == -1 ) {
		fp = fopen(file_name, "w");
		if (fp == NULL)	{
			fprintf(stderr, "Error creating [%s]\n",file_name);
			return -1;
		}

		/* Default to 0 */
		data = 0;
		fwrite(&data, sizeof(data), 1, fp);
		memcpy(buffer, &data, sizeof(data));
		fclose(fp);
		return 0;
	}

	/* This must be a re-run */
	fp = fopen(file_name, "r");
	if (fp == NULL)	{
		fprintf(stderr, "Error opening [%s]\n",file_name);
		return -1;
	}

	if (fread(buffer, len, 1, fp) != 1) {
		fprintf(stderr, "Error reading [%s]\n",file_name);
		fclose(fp);
		return -1;
	}

	return 0;
}

/* Generic file writer */
int write_data(const char *file_name, void *buffer, size_t len)
{
	int r = 0;
	FILE *fp = NULL;

	fp = fopen(file_name, "w");
	if (fp == NULL)	{
		fprintf(stderr, "Error opening [%s]\n",file_name);
		return -1;
	}

	r = (fwrite(buffer, len, 1, fp) != 1) ? -1 : 0;
	fclose(fp);
	return r;
}

/*
 * Accepts a settings name and returns its value. It only works
 * for the variant of type 'string' now.
 */
char *get_system_settings(const char *user_setting)
{
	int r = -1;
	const char *settings_host_bus = "org.openbmc.settings.Host";
	const char *settings_host_object = "/org/openbmc/settings/host0";
	const char *settings_host_intf = "org.freedesktop.DBus.Properties";

	const char *value = NULL;
	char *settings_value = NULL;

	sd_bus_message *reply = NULL;
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	r = sd_bus_call_method(g_time_bus,
			settings_host_bus,
			settings_host_object,
			settings_host_intf,
			"Get",
			&bus_error,
			&reply,
			"ss",
			settings_host_bus,
			user_setting);
	if (r < 0) {
		fprintf(stderr, "Error [%s] reading system settings\n",
				strerror(-r));
		goto finish;
	}

	r = sd_bus_message_read(reply, "v", "s", &value);
	if(r < 0)
		fprintf(stderr, "Error [%s] parsing settings data\n",
				strerror(-r));
finish:
	if (value)
		settings_value = strdup(value);
	reply = sd_bus_message_unref(reply);
	sd_bus_error_free(&bus_error);

	return settings_value;
}

/* Reads PGOOD value from org.openbmc.control.Power */
int get_pgood_value()
{
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;

	int r = 0;
	int pgood = -1;

	r = sd_bus_call_method(g_time_bus,
			"org.openbmc.control.Power",
			"/org/openbmc/control/power0",
			"org.freedesktop.DBus.Properties",
			"Get",
			&bus_error,
			&reply,
			"ss",
			"org.openbmc.control.Power",
			"pgood");
	if (r < 0) {
		fprintf(stderr, "Error [%s] reading pgood value\n",
				strerror(-r));
		goto finish;
	}

	r = sd_bus_message_read(reply, "v", "i", &pgood);
	if (r < 0)
		fprintf(stderr, "Error [%s] parsing pgood data\n",
				strerror(-r));
finish:
	sd_bus_error_free(&bus_error);
	reply = sd_bus_message_unref(reply);

	return pgood;
}
