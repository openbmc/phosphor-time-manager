# Intro
`phosphor-time-manager` is the time manager service that implements dbus
interface `xyz/openbmc_project/Time/EpochTime.interface.yaml`, user can get or
set the BMC or HOST's time via this interface.

### General usage
The service `xyz.openbmc_project.Time.Manager` provides two objects on dbus:
* /xyz/openbmc_project/Time/Bmc
* /xyz/openbmc_project/Time/Host

where each object implements interface `xyz.openbmc_project.Time.EpochTime`.

User can directly get or set the property `Elasped` of the objects to get or set
the time. For example:

* To get BMC's time:
   ```
   ### With busctl on BMC
   busctl get-property xyz.openbmc_project.Time.Manager \
       /xyz/openbmc_project/Time/Bmc xyz.openbmc_project.Time.EpochTime Elapsed
   
   ### With REST API on remote host
   curl -b cjar -k https://bmc-ip/xyz/openbmc_project/Time/Bmc
   ```
* To set HOST's time:
   ```
   ### With busctl on BMC
   busctl set-property xyz.openbmc_project.Time.Manager \
       /xyz/openbmc_project/Time/Host xyz.openbmc_project.Time.EpochTime \
       Elapsed t <value>
   
   ### With REST API on remote host
   ### TODO: this is not working as the type is NOT parsed as uint64
   curl -b cjar -k -H "Content-Type: application/json" -X PUT \
       -d '{"data": {"Elapsed": 1487304700000000}}' \
       https://bmc-ip/xyz/openbmc_project/Time/Host
   ```

### Time settings
Getting BMC or HOST time is always allowed, but setting the time may not be
allowed, depending on below two settings in `phosphor-settingsd`.

* time_mode
   * NTP: Time is set via NTP server.
   * MANUAL: Time is set manually.
* time_owner
   * BMC: BMC can set the time.
   * HOST: Host OS can set the time.
   * SPLIT: BMC and Host owns separated time.
   * BOTH: Both BMC and Host can set the time.

A summary of in which cases the time can be set on BMC or HOST:
Mode      | Owner | Set BMC Time  | Set Host Time
--------- | ----- | ------------- | -------------------
NTP       | BMC   | Not allowed   | Not allowed
NTP       | HOST  | Not allowed   | Invalid case
NTP       | SPLIT | Not allowed   | OK
NTP       | BOTH  | Not allowed   | OK
MANUAL    | BMC   | OK            | Not allowed
MANUAL    | HOST  | Not allowed   | OK
MANUAL    | SPLIT | OK            | OK
MANUAL    | BOTH  | OK            | OK

### Pgood
When host is on (pgood == 1), the changes of the above time mode/owner are not
applied but deferred. The changes of the mode/owner are saved to persistent
storage. 

When host becomes off (pgood == 0), the saved mode/owner are read from
persistent storage and are applied.
