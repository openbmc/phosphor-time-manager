#include <string.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <mapper.h>
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
	const char *settings_host_obj = "/org/openbmc/settings/host0";
	const char *property_intf = "org.freedesktop.DBus.Properties";
	const char *host_intf = "org.openbmc.settings.Host";

	const char *value = NULL;
	char *settings_value = NULL;

	/* Get the provider from object mapper */
	char *settings_provider = NULL;

	sd_bus_message *reply = NULL;
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	settings_provider = get_provider(settings_host_obj);
	if (!settings_provider)
		return NULL;

	r = sd_bus_call_method(g_time_bus,
			settings_provider,
			settings_host_obj,
			property_intf,
			"Get",
			&bus_error,
			&reply,
			"ss",
			host_intf,
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

	free(settings_provider);
	return settings_value;
}

/* Reads PGOOD value from /org/openbmc/control/power0 */
int get_pgood_value()
{
	int r = 0;
	const char *pgood_obj = "/org/openbmc/control/power0";
	const char *property_intf = "org.freedesktop.DBus.Properties";
	const char *pgood_intf = "org.openbmc.control.Power";

	int pgood = -1;

	/* Get the provider from object mapper */
	char *pgood_provider = NULL;

	sd_bus_error bus_error = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;

	pgood_provider = get_provider(pgood_obj);
	if (!pgood_provider)
		return -1;

	r = sd_bus_call_method(g_time_bus,
			pgood_provider,
			pgood_obj,
			property_intf,
			"Get",
			&bus_error,
			&reply,
			"ss",
			pgood_intf,
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
	free(pgood_provider);

	return pgood;
}

/* Updates .network file with UseNtp= */
int updt_network_settings(const char *use_dhcp_ntp)
{
	int r = -1;
	const char *network_obj = "/org/openbmc/NetworkManager/Interface";
	const char *network_intf = "org.openbmc.NetworkManager";

	/* Get the provider from object mapper */
	char *network_provider = NULL;
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	network_provider = get_provider(network_obj);
	if (!network_provider)
		return -1;

	r = sd_bus_call_method(g_time_bus,
			network_provider,
			network_obj,
			network_intf,
			"UpdateUseNtpField",
			&bus_error,
			NULL,
			"s",
			use_dhcp_ntp);
	if (r < 0)
		fprintf(stderr, "Error:[%s] updating UseNtp in .network file\n",
				strerror(-r));
	else
		printf("Successfully updated UseNtp=[%s] in .network files\n",
				use_dhcp_ntp);

	sd_bus_error_free(&bus_error);
	free(network_provider);
	return r;
}

/* Returns the busname that hosts obj_path */
char *get_provider(const char *obj_path)
{
	int r = 0;
	char *provider = NULL;

	r = mapper_get_service(g_time_bus, obj_path, &provider);
	if (r < 0)
		fprintf(stderr, "Error [%s] getting bus name for [%s] provider\n",
				strerror (-r), obj_path);
	return provider;
}
