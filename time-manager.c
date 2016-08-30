#define __USE_XOPEN
#define _GNU_SOURCE
/*
 * manpage of strptime tells to define _XOPEN_SOURCE but
 * that does not make strptime happy until it finds these above.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/timerfd.h>
#include <systemd/sd-event.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include "time-utils.h"

/*
 * Neeed to do this since its not exported outside of the kernel.
 * Refer : https://gist.github.com/lethean/446cea944b7441228298
 */
#ifndef TFD_TIMER_CANCEL_ON_SET
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

/*
 * Needed to make sure timerfd does not misfire eventhough we set CANCEL_ON_SET
 */
#define TIME_T_MAX (time_t)((1UL << ((sizeof(time_t) << 3) - 1)) - 1)

/* Needed to use freedesktop/timedate1 services */
#define USEC_PER_SEC  ((uint64_t) 1000000ULL)

const char *WATCH_SETTING_CHANGE = "type='signal',interface='org.freedesktop.DBus.Properties',"
				   "path='/org/openbmc/settings/host0',member='PropertiesChanged'";

const char *WATCH_PGOOD_CHANGE = "type='signal',interface='org.freedesktop.DBus.Properties',"
				 "path='/org/openbmc/control/power0',member='PropertiesChanged'";

sd_bus *g_time_bus = NULL;
sd_bus_slot *g_time_slot = NULL;

/* Globals to support multiple modes and owners */
time_mode_t  g_curr_time_mode = NTP;
time_mode_t  g_requested_time_mode;

time_owner_t g_curr_time_owner = BMC;
time_owner_t g_requested_time_owner;

/* Used when owner is 'SPLIT' */
time_t g_host_time_offset = 0;
time_t g_curr_host_time = 0;
bool g_time_settings_changes_allowed = false;

/* OpenBMC Time Manager dbus framework */
const char *time_bus_name =  "org.openbmc.TimeManager";
const char *time_obj_path  =  "/org/openbmc/TimeManager";
const char *time_intf_name =  "org.openbmc.TimeManager";

const char *host_time_file = "/var/lib/obmc/host_time";
const char *host_offset_file = "/var/lib/obmc/host_offset";

const char *time_mode_file = "/var/lib/obmc/time_mode";
const char *time_owner_file = "/var/lib/obmc/time_owner";
/*
 * Reads timeofday and returns time string as %Y-%m-%d %H:%M:%S
 * and also number of seconcd.
 */
static int get_bmc_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
	time_t current_bmc_time = 0;

	/* Enough for '%Y-%m-%d %H:%M:%S' */
	char time_str[28] = {0};

	printf("Request to get BMC time\n");

	time(&current_bmc_time);

	strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", localtime(&current_bmc_time));

	return sd_bus_reply_method_return(m, "xs", (int64_t)current_bmc_time, &time_str);
}

/*
 * Designated to be called by IPMI_GET_SEL time from host
 */
static int get_host_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
	time_t curr_host_time = 0;
	time_t curr_bmc_time = 0;

	/* Get the time on BMC and adjust the offset */
	time(&curr_bmc_time);
	if (g_curr_time_owner == SPLIT)
		curr_host_time = curr_bmc_time + g_host_time_offset;
	else
		curr_host_time = curr_bmc_time;

	printf("get_host_time: host_time:[%ld], bmc_time:[%ld], host_offset:[%ld]\n",
			curr_host_time, curr_bmc_time, g_host_time_offset);

	return sd_bus_reply_method_return(m, "x", (int64_t)curr_host_time);
}

/* Helper function to set the time. */
int set_time_of_day(const time_t t_of_day)
{
	int r = 0;
	int64_t new_time_usec = 0;
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	/*
	 * These 2 are for  bypassing some policy
	 * checking in the timedate1 service
	 */
	bool relative = false;
	bool interactive = false;

	/* timedate1 service requires that the time be passed as usec */
	new_time_usec = (int64_t)t_of_day * USEC_PER_SEC;

	r = sd_bus_call_method(g_time_bus,
			"org.freedesktop.timedate1",
			"/org/freedesktop/timedate1",
			"org.freedesktop.timedate1",
			"SetTime",
			&bus_error,
			NULL,        /* timedate1 does not return response */
			"xbb",
			(int64_t)new_time_usec,
			relative,
			interactive);

	sd_bus_error_free(&bus_error);
	return r;
}

/*
 * Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
 * and then sets the BMC time. If the input time string does not conform to the
 * format, an error message is returned.
 */
static int set_bmc_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
	struct tm user_tm = {0};
	time_t t_of_day = 0;
	int r = -1;

	const char *user_time_str = NULL;
	const char *time_val = NULL;
	const char *return_msg = NULL;

	printf("Calling BMC_SET_TIME\n");

	printf("set_bmc_time: curr_mode:[%d-%s] curr_owner[%d-%s]",
			g_curr_time_mode, mode_str(g_curr_time_mode),
			g_curr_time_owner, owner_str(g_curr_time_owner));

	if (g_curr_time_mode == NTP) {
		return_msg = "Can not set Time. NTP is enabled";
		goto finish;
	}

	if (g_curr_time_owner == HOST) {
		return_msg = "Can not set time. Time Owner is HOST";
		goto finish;
	}

	r = sd_bus_message_read(m, "s", &user_time_str);
	if (r < 0) {
		fprintf(stderr,"Error [%s] reading user time\n",strerror(-r));
		goto finish;
	}

	/* Forced to type case to make compiler happy */
	time_val = (char *)strptime(user_time_str, "%Y-%m-%d %H:%M:%S", &user_tm);
	if (*time_val != '\0' || time_val == NULL) {
		r = -1;
		return_msg = "Format error. Use '%%Y-%%m-%%d %%H:%%M:%%S'";
		goto finish;
	}

	/* Convert the time structure into number of seconds */
	t_of_day = mktime(&user_tm);
	if (t_of_day < 0) {
		r = -1;
		fprintf(stderr,"Error converting tm to seconds\n");
		goto finish;
	}

	/* Set REALTIME and also update hwclock */
	r = set_time_of_day(t_of_day);
	if (r < 0)
		fprintf(stderr, "Error [%s] setting time on BMC",strerror(-1));

finish:
	if (!return_msg)
		return_msg = r < 0 ? "Failure setting time" : "Success";

	return sd_bus_reply_method_return(m, "is", r, return_msg);
}

/*
 * Gets the time string and verifies if it conforms to format %Y-%m-%d %H:%M:%S
 * and then sets the BMC time. If the input time string does not conform to the
 * format, an error message is returned.
 */
static int set_host_time(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
	time_t curr_bmc_time = 0;
	time_t new_host_time = 0;

	const char *return_msg = NULL;

	/*
	 * We should not return failure to host just becase
	 * policy does not allow it
	 */
	int r = 0;

	printf("set_host_time: curr_mode:[%d-%s] curr_owner:[%d-%s], curr_host_time:[%ld],"
			" host_offset:[%ld]\n", g_curr_time_mode, mode_str(g_curr_time_mode),
			g_curr_time_owner,owner_str(g_curr_time_owner),
			g_curr_host_time, g_host_time_offset);

	if (g_curr_time_owner == BMC) {
		return_msg = "Can not set time. Current time owner is BMC";
		goto finish;
	}

	r = sd_bus_message_read(m, "x", &new_host_time);
	if (r < 0) {
		fprintf(stderr,"Error [%s] reading host time\n",strerror(-r));
		goto finish;
	}

	printf("set_host_time: new_host_time:[%ld]\n",new_host_time);

	if (g_curr_time_owner == SPLIT) {
		g_curr_host_time = new_host_time;

		/* Adjust the host offset */
		time(&curr_bmc_time);
		g_host_time_offset = g_curr_host_time - curr_bmc_time;

		/* Persist this new one */
		r = write_data(host_time_file, &g_curr_host_time,
				sizeof(g_curr_host_time));
		if (r < 0)
			fprintf(stderr, "Error saving host_time:[%ld]\n",
					g_curr_host_time);
		/* FIXME : Does it make sense to abort ? */

		r = write_data(host_offset_file, &g_host_time_offset,
				sizeof(g_host_time_offset));
		if (r < 0)
			fprintf(stderr, "Error saving host_offset:[%ld]\n",
					g_host_time_offset);

		/* We are not doing any time settings in BMC */
		printf("set_host_time: Updated: host_time:[%ld], host_offset:[%ld]\n",
			g_curr_host_time, g_host_time_offset);

		goto finish;
	}

	/*
	 * We are okay to update time in as long as BMC is not the owner
	 */
	r = set_time_of_day(new_host_time);

finish:
	if (return_msg == NULL)
		return_msg = r < 0 ? "Failure setting time" : "Success";

	return sd_bus_reply_method_return(m, "is", r, return_msg);
}

/* Function pointer of APIs exposed via Dbus */
static const sd_bus_vtable time_services_vtable[] =
{
	SD_BUS_VTABLE_START(0),
	SD_BUS_METHOD("SetBMCTime", "s", "is", &set_bmc_time, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("SetHostTime", "x", "is", &set_host_time, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetBMCTime", "", "xs", &get_bmc_time, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_METHOD("GetHostTime", "", "x", &get_host_time, SD_BUS_VTABLE_UNPRIVILEGED),
	SD_BUS_VTABLE_END,
};

/* Gets called into  by sd_event  on an activity seen on sd_bus */
static int process_sd_bus_message(sd_event_source *es, int fd,
				uint32_t revents, void *userdata)
{
	int r = sd_bus_process(g_time_bus, NULL);
	if (r < 0)
		printf("Error [%s] processing sd_bus message:\n",
				strerror(-r));
	return r;
}

/* Gets called into by sd_event on any time SET event */
static int process_time_change(sd_event_source *es, int fd,
				uint32_t revents, void *userdata)
{
	char buffer[64] = {0};
	time_t curr_bmc_time = 0;
	int r = 0;

	/*
	 *  We are not interested in the data here. Need to read time again .
	 *  So read until there is something here in the FD
	 */
	while (read(fd, &buffer, 64) > 0);

	printf("process_time_change: curr_mode:[%d-%s] curr_owner:[%d-%s],"
			" curr_host_time:[%ld], host_offset:[%ld]\n",
			g_curr_time_mode, mode_str(g_curr_time_mode),
			g_curr_time_owner, owner_str(g_curr_time_owner),
			g_curr_host_time, g_host_time_offset);
	/*
	 * Read the current BMC time and adjust the
	 * host time offset if the mode is  SPLIT
	 */
	if (g_curr_time_owner == SPLIT) {
		time(&curr_bmc_time);
		g_host_time_offset = g_curr_host_time - curr_bmc_time;

		printf("process_time_change: Updated: curr_bmc_time:[%ld],"
				" curr_host_time:[%ld], host_offset:[%ld]\n",
				curr_bmc_time, g_curr_host_time, g_host_time_offset);

		/* Persist this */
		r = write_data(host_offset_file, &g_host_time_offset,
				sizeof(g_host_time_offset));
		if (r < 0)
			fprintf(stderr, "Error saving host_offset:[%ld]\n",
					g_host_time_offset);
	}

	return r;
}

/* Accepts the time mode and makes necessary changes to timedate1 */
int modify_ntp_settings(const time_mode_t new_time_mode)
{
	int r = -1;
	int ntp_change_op = 0;

	// Error return mechanism
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	/* Pass '1' -or- '0' to SetNTP method indicating Enable/Disable */
	ntp_change_op = (new_time_mode == NTP) ? 1 : 0;

	printf("Applying NTP setting..\n");

	r = sd_bus_call_method(g_time_bus,
			"org.freedesktop.timedate1",
			"/org/freedesktop/timedate1",
			"org.freedesktop.timedate1",
			"SetNTP",
			&bus_error,
			NULL,    /* timedate1 does not return response */
			"bb",
			ntp_change_op,       /* '1' means Enable */
			0);      	     /* '0' meaning no policy-kit */
	if (r < 0)
		printf("Error [%s] changing time Mode\n", strerror(-r));
	else {
		printf("SUCCESS. NTP setting is now:[%s]\n",
				(ntp_change_op) ? "Enabled" : "Disabled");

		if (ntp_change_op)
			system("systemctl restart systemd-timesyncd &");
		else
			system("systemctl stop systemd-timesyncd &");
	}
	return r;
}

/* Manipulates time owner if the system setting allows it */
int updt_time_mode(const time_mode_t time_mode)
{
	int r = 0;

	printf("Requested_Mode:[%d-%s], Current_Mode:[%d-%s]\n",
			time_mode, mode_str(time_mode),
			g_curr_time_mode, mode_str(g_curr_time_mode));

	if (time_mode == g_curr_time_mode) {
		printf("Mode is already set to :[%d-%s]\n",
				time_mode, mode_str(time_mode));
		return 0;
	}

	/*
	 * Also, if the time owner is HOST, then we should not allow NTP.
	 * However, it may so happen that there are 2 pending requests, one for
	 * changing to NTP and other for changing owner to something not HOST.
	 * So check if there is a pending time_owner change and if so, allow NTP
	 * if the current is HOST and requested is non HOST.
	 */
	if (g_curr_time_owner == HOST &&
	    	g_requested_time_owner == HOST &&
	    	time_mode == NTP) {
			printf("Can not set mode to NTP with HOST as owner\n");
			return 0;
	}

	if (g_time_settings_changes_allowed) {
		r = modify_ntp_settings(time_mode);
		if (r < 0)
			fprintf(stderr, "Error changing TimeMode settings");
		else
			g_curr_time_mode = time_mode;

		printf("Current_Mode changed to:[%d-%s]\n", g_curr_time_mode,
				mode_str(g_curr_time_mode));

		/* Need this when we either restart or come back from reset */
		r = write_data(time_mode_file, &g_curr_time_mode,
				sizeof(&g_curr_time_mode));
	} else {
		printf("Deferring update until system state allows it\n");
	}

	return r;
}

/* Manipulates time owner if the system setting allows it */
int updt_time_owner(const time_owner_t time_owner)
{
	int r = 0;

	/* Needed when owner changes to HOST */
	time_mode_t forced_mode_manual = MANUAL;

	if (time_owner == g_curr_time_owner) {
		printf("Owner is already set to :[%d-%s]\n",
				time_owner, owner_str(time_owner));
		return 0;
	}

	printf("Requested_Owner:[%d-%s], Current_Owner:[%d-%s]\n",
			time_owner, owner_str(time_owner),
			g_curr_time_owner, owner_str(g_curr_time_owner));

	if (g_time_settings_changes_allowed) {
		/*
		 * If we are transitioning from SPLIT to something else,
		 * reset the host offset.
		 */
		if (g_curr_time_owner == SPLIT &&
			g_requested_time_owner != SPLIT) {
			g_host_time_offset = 0;
			g_curr_host_time = 0;

			/* Persist this new one */
			/* FIXME : Does it make sense to abort on err */
			r = write_data(host_time_file, &g_curr_host_time,
			  		sizeof(g_host_time_offset));
			if (r < 0)
				fprintf(stderr, "Error saving host_time:[%ld]\n",
						g_curr_host_time);

			r = write_data(host_offset_file, &g_host_time_offset,
					sizeof(g_host_time_offset));
			if (r < 0)
				fprintf(stderr, "Error saving host_offset:[%ld]\n",
						g_host_time_offset);
		}
		g_curr_time_owner = time_owner;

		printf("Current_Owner is now:[%d-%s]\n",g_curr_time_owner,
				owner_str(g_curr_time_owner));

		/* If owner is HOST, then we need to force mode to MANUAL */
		if (g_curr_time_owner == HOST) {
			printf("Forcing the mode to MANUAl\n");
			r = updt_time_mode(forced_mode_manual);
			if (r < 0) {
				fprintf(stderr, "Error forcing the mode to MANUAL\n");
				return r;
			}
		}

		/* Need this when we either restart or come back from reset */
		r = write_data(time_owner_file, &g_curr_time_owner,
				sizeof(g_curr_time_owner));
	} else {
		printf("Deferring update until system state allows it\n");
	}

	return r;
}

/* Updates .network file with UseNtp= */
int updt_network_settings(const char *use_dhcp_ntp)
{
	int r = -1;
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	r = sd_bus_call_method(g_time_bus,
			"org.openbmc.NetworkManager",
			"/org/openbmc/NetworkManager/Interface",
			"org.openbmc.NetworkManager",
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
	return r;
}

/*
 * Called by sd_event when Properties are changed in Control/power0
 * Interested in change to 'pgood'
 */
int process_pgood_change(const int new_pgood)
{
	int r = 0;

	/* Indicating that we are safe to apply any changes */
	if (new_pgood == 0) {
		g_time_settings_changes_allowed = true;
		printf("Changing the time settings allowed now\n");
	} else {
		g_time_settings_changes_allowed = false;
		printf("Changing the time settings is *NOT* allowed now\n");
	}

	/*
	 * if we have had users that changed the time settings
	 * when we were not ready yet, do it now.
	 */
	if (g_requested_time_owner != g_curr_time_owner) {
		r = updt_time_owner(g_requested_time_owner);
		if (r < 0) {
			fprintf(stderr, "Error updating new time owner\n");
			return r;
		}
		printf("New time_owner is :[%d-%s]\n", g_requested_time_owner,
				owner_str(g_requested_time_owner));
	}

	if (g_requested_time_mode != g_curr_time_mode) {
		r = updt_time_mode(g_requested_time_mode);
		if (r < 0) {
			fprintf(stderr, "Error updating new time mode\n");
			return r;
		}
		printf("New time mode is :[%d-%s]\n", g_requested_time_mode,
				mode_str(g_requested_time_mode));
	}

	return 0;
}

/*
 * Called by sd_event when Properties are changed in settingsd.
 * Interested in changes to 'time_mode', 'time_owner' and 'use_dhcp_ntp'
 */
static int process_property_change(sd_bus_message *m, void *user_data,
				   sd_bus_error *ret_error)
{
	const char *key = NULL;
	const char *new_time_mode = NULL;
	const char *new_time_owner = NULL;
	const char *use_dhcp_ntp = NULL;

	int new_pgood = -1;
	int r = -1;

	printf("Settings CHANGED\n");

	/* input data is "sa{sv}as" and we are just interested in a{sv} */
	r = sd_bus_message_skip(m, "s");
	if (r < 0) {
		fprintf(stderr, "Error [%s] skipping interface name in data\n",
				strerror(-r));
		goto finish;
	}

	r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
	if (r < 0) {
		fprintf(stderr, "Error [%s] entering the dictionary\n",
				strerror(-r));
		goto finish;
	}

	while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0) {
		r = sd_bus_message_read(m, "s", &key);
		if (r < 0) {
			fprintf(stderr, "Error [%s] reading the key from dict\n",
					strerror(-r));
			/*
			 * Can not continue here since the next
			 * enter would result in error anyway
			 */
			goto finish;
		}

		if (!strcmp(key, "time_mode")) {
			r = sd_bus_message_read(m, "v", "s", &new_time_mode);
			if (r < 0) {
				fprintf(stderr, "Error [%s] reading time_mode\n",
						strerror(-r));
				goto finish;
			}
		}
		else if (!strcmp(key, "time_owner")) {
			r = sd_bus_message_read(m, "v", "s", &new_time_owner);
			if (r < 0) {
				fprintf(stderr, "Error [%s] reading time_owner\n",
						strerror(-r));
				goto finish;
			}
		}
		else if (!strcmp(key, "use_dhcp_ntp")) {
			r = sd_bus_message_read(m, "v", "s", &use_dhcp_ntp);
			if (r < 0) {
				fprintf(stderr, "Error [%s] reading use_dhcp_ntp\n",
						strerror(-r));
				goto finish;
			}
		}
		else if (!strcmp(key, "pgood")) {
			r = sd_bus_message_read(m, "v", "i", &new_pgood);
			if (r < 0) {
				fprintf(stderr, "Error [%s] reading pgood\n",
						strerror(-r));
				goto finish;
			}
		}
		else
			sd_bus_message_skip(m, "v");
	}

	/*
	 * Need to verify if the requested values can be honored now. Else those
	 * will be honored only when we change the system state to BMC_READY and
	 * HOST_POWERED_OFF
	 */
	if (new_time_mode) {
		g_requested_time_mode = get_time_mode(new_time_mode);
		r = updt_time_mode(g_requested_time_mode);
		if (r < 0) {
			fprintf(stderr, "Error updating new time mode\n");
			goto finish;
		}
	}

	if (new_time_owner) {
		g_requested_time_owner = get_time_owner(new_time_owner);
		r = updt_time_owner(g_requested_time_owner);
		if (r < 0) {
			fprintf(stderr, "Error updating new time owner\n");
			goto finish;
		}
	}

	/* This will make updates to .network file */
	if (use_dhcp_ntp) {
		r = updt_network_settings(use_dhcp_ntp);
		if (r < 0) {
			fprintf(stderr, "Error updating network files \n");
			goto finish;
		}
		printf("New use_dhcp_ntp is :[%s]\n", use_dhcp_ntp);
	}

	/*
	 * if we have had a new pgood value, then apply any
	 * pending policy changes
	 */
	if (new_pgood != -1) {
		r = process_pgood_change(new_pgood);
		if (r < 0) {
			fprintf(stderr, "Error updating pending policy changes");
		}
	}
finish:
	return r;
}

/*
 *  Reads the values from 'settingsd' and applies:
 *  1) Time Mode
 *  2) time Owner
 *  3) UseNTP setting
 */
int process_initial_settings(void)
{
	char *initial_time_mode = NULL;
	char *initial_time_owner = NULL;
	char *initial_dhcp_ntp = NULL;

	int init_pgood = -1;
	int r = -1;

	/*
	 * Before starting anything, need to read the
	 * 'time_owner' and 'time_mode' * from settings.
	 */
	initial_time_mode = get_system_settings("time_mode");
	initial_time_owner = get_system_settings("time_owner");
	initial_dhcp_ntp = get_system_settings("use_dhcp_ntp");

	printf("Initial-> time_mode:[%s], time_owner:[%s], use_dhcp_ntp:[%s]\n",
		initial_time_mode, initial_time_owner, initial_dhcp_ntp);

	if (initial_time_mode == NULL ||
	    initial_time_owner == NULL ||
	    initial_dhcp_ntp == NULL) {
		fprintf(stderr, "Error reading initial settings\n");
		goto finish;
	}

	/*
	 * Get the pgood value now so that we can set
	 * the allowed to change value
	 */
	init_pgood = get_pgood_value();
	if (init_pgood == 0) {
		g_time_settings_changes_allowed = true;
		printf("Changing the time settings allowed now\n");
	} else {
		g_time_settings_changes_allowed = false;
		printf("Changing the time settings is *NOT* allowed now\n");
	}

	/* Get current current host_time */
	r = read_data(host_time_file, &g_curr_host_time,
			sizeof(g_curr_host_time));
	if (r < 0) {
		fprintf(stderr, "Error reading current host time\n");
		goto finish;
	}
	printf("Last known host_time:[%ld]\n",g_curr_host_time);

	/* Get current host_offset */
	r = read_data(host_offset_file, &g_host_time_offset,
			sizeof(g_host_time_offset));
	if (r < 0) {
		fprintf(stderr, "Error reading current host offset\n");
		goto finish;
	}
	printf("Last known host_offset:[%ld]\n",g_host_time_offset);

	/*
	 * If we are coming back from a reset reload, then need to
	 * read what was the last successful Mode and Owner.
	 */
	r = read_data(time_mode_file, &g_curr_time_mode,
			sizeof(g_curr_time_mode));
	if (r < 0) {
		fprintf(stderr, "Error reading current time mode\n");
		goto finish;
	}
	printf("Last known time_mode:[%d-%s]\n",g_curr_time_mode,
			mode_str(g_curr_time_mode));

	r = read_data(time_owner_file, &g_curr_time_owner,
			sizeof(g_curr_time_owner));
	if (r < 0) {
		fprintf(stderr, "Error reading current time owner\n");
		goto finish;
	}
	printf("Last known time_owner:[%d-%s]\n", g_curr_time_owner,
			owner_str(g_curr_time_owner));

	/*
	 * Now that we have verified system states,
	 * we can go ahead update others
	 */
	g_requested_time_owner = get_time_owner(initial_time_owner);
	r = updt_time_owner(g_requested_time_owner);
	if (r < 0) {
		fprintf(stderr,"Error setting time owner\n");
		goto finish;
	}

	g_requested_time_mode = get_time_mode(initial_time_mode);
	r = updt_time_mode(g_requested_time_mode);
	if (r < 0) {
		fprintf(stderr,"Error setting time mode\n");
		goto finish;
	}

	r = updt_network_settings(initial_dhcp_ntp);
	if (r < 0)
		fprintf(stderr,"Error updating use_dhcp_ntp\n");

finish:
	free(initial_time_mode);
	free(initial_time_owner);
	free(initial_dhcp_ntp);
	return r;
}

/*
 * Sets up callback handlers for activities on :
 * 1) user request on SD_BUS
 * 2) Time change
 * 3) Settings change
 * 4) System state change;
 */
int register_callback_handlers(sd_event *event, sd_event_source *event_source)
{
	int sd_bus_fd = -1;
	int time_fd = -1;
	int r = -1;

	/* Choose the MAX time that is possible to aviod mis fires. */
	struct itimerspec max_time = {
		.it_value.tv_sec = TIME_T_MAX,
	};

	/* Extract the descriptor out of sd_bus construct. */
	sd_bus_fd = sd_bus_get_fd(g_time_bus);
	r = sd_event_add_io(event, &event_source, sd_bus_fd, EPOLLIN, process_sd_bus_message, NULL);
	if (r < 0) {
		fprintf(stderr, "Error [%s] adding sd_bus_message handler\n",
				strerror(-r));
		return r;
	}

	/* Wake me up *only* if someone SETS the time */
	time_fd = timerfd_create(CLOCK_REALTIME,0);
	timerfd_settime(time_fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &max_time, NULL);

	r = sd_event_add_io(event, &event_source, time_fd, EPOLLIN, process_time_change, NULL);
	if (r < 0) {
		fprintf(stderr, "Error [%s] adding time_change handler\n",
				strerror(-r));
		return r;
	}

	/* Watch for property changes in settingsd */
	r = sd_bus_add_match(g_time_bus, &g_time_slot, WATCH_SETTING_CHANGE, process_property_change, NULL);
	if (r < 0) {
		fprintf(stderr, "Error [%s] adding property change listener\n",
				strerror(-r));
		return r;
	}

	/*
	 * Watch for state change. Only reliable one to count on is
	 * state of [pgood]. value of [1] meaning host is powering on / powered
	 * on. [0] means powered off.
	 */
	r = sd_bus_add_match(g_time_bus, &g_time_slot, WATCH_PGOOD_CHANGE, process_property_change, NULL);
	if (r < 0)
		fprintf(stderr, "Error [%s] adding pgood change listener\n",
				strerror(-r));
	return r;
}

int main(int argc, char *argv[])
{
	sd_event_source *event_source = NULL;
	sd_event *event = NULL;

	int r = -1;

	printf("OpenBMC Time Manager Starting....\n");
	r = sd_bus_default_system(&g_time_bus);
	if (r < 0) {
		fprintf(stderr, "Error [%s] connecting to system bus\n",
				strerror(-r));
		goto finish;
	}

	printf("Registering dbus methods\n");
	r = sd_bus_add_object_vtable(g_time_bus,
			&g_time_slot,
			time_obj_path,
			time_bus_name,
			time_services_vtable,
			NULL);
	if (r < 0) {
		fprintf(stderr, "Error [%s] adding timer services vtable\n",
				strerror(-r));
		goto finish;
	}

	printf("Requesting dbus name:[%s]\n", time_bus_name);
	r = sd_bus_request_name(g_time_bus, time_bus_name, 0);
	if (r < 0) {
		fprintf(stderr, "Error [%s] acquiring service name\n",
				strerror(-r));
		goto finish;
	}

	/* create a sd_event object and add handlers */
	r = sd_event_default(&event);
	if (r < 0) {
		fprintf(stderr, "Error [%s] creating an sd_event",strerror(-r));
		goto finish;
	}

	/*
	 * Handlers called by sd_event when an activity
	 * is observed in event  loop
	 */
	r = register_callback_handlers(event, event_source);
	if (r < 0) {
		fprintf(stderr, "Error setting up callback handlers\n");
		goto finish;
	}

	/* Reads the properties from settings and applies changes if needed */
	r = process_initial_settings();
	if (r < 0) {
		fprintf(stderr, "Error processing initial settings\n");
		goto finish;
	}

	/* Wait for the work */
	r = sd_event_loop(event);

finish:
	event_source = sd_event_source_unref(event_source);
	event = sd_event_unref(event);

	if (r < 0)
		fprintf(stderr, "Failure: %s\n", strerror(-r));

	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
