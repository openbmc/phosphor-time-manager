# Introduction
`phosphor-time-manager` is the time manager service that implements dbus
interface `xyz/openbmc_project/Time/EpochTime.interface.yaml`.
User can get or set the BMC's or HOST's time via this interface.

### General usage
The service `xyz.openbmc_project.Time.Manager` provides two objects on dbus:
* /xyz/openbmc_project/time/bmc
* /xyz/openbmc_project/time/host

where each object implements interface `xyz.openbmc_project.Time.EpochTime`.

User can directly get or set the property `Elasped` of the objects to get or set
the time. For example on an authenticated session:

* To get BMC's time:
   ```
   ### With busctl on BMC
   busctl get-property xyz.openbmc_project.Time.Manager \
       /xyz/openbmc_project/time/bmc xyz.openbmc_project.Time.EpochTime Elapsed

   ### With REST API on remote host
   curl -b cjar -k https://bmc-ip/xyz/openbmc_project/time/bmc
   ```
* To set HOST's time:
   ```
   ### With busctl on BMC
   busctl set-property xyz.openbmc_project.Time.Manager \
       /xyz/openbmc_project/time/host xyz.openbmc_project.Time.EpochTime \
       Elapsed t <value-in-microseconds>

   ### With REST API on remote host
   curl -b cjar -k -H "Content-Type: application/json" -X PUT \
       -d '{"data": 1487304700000000}' \
       https://bmc-ip/xyz/openbmc_project/time/host/attr/Elapsed
   ```

### Time settings
Getting BMC or HOST time is always allowed, but setting the time may not be
allowed depending on below two settings in settings manager.

* time_mode
   * NTP: Time is set via NTP server.
   * MANUAL: Time is set manually.
* time_owner
   * BMC: BMC owns the time and can set the time.
   * HOST: Host owns the time and can set the time.
   * SPLIT: BMC and Host owns separate time.
   * BOTH: Both BMC and Host can set the time.

A summary of which cases the time can be set on BMC or HOST:
Mode      | Owner | Set BMC Time  | Set Host Time
--------- | ----- | ------------- | -------------------
NTP       | BMC   | Not allowed   | Not allowed
NTP       | HOST  | Not allowed   | Not allowed
NTP       | SPLIT | Not allowed   | OK
NTP       | BOTH  | Not allowed   | Not allowed
MANUAL    | BMC   | OK            | Not allowed
MANUAL    | HOST  | Not allowed   | OK
MANUAL    | SPLIT | OK            | OK
MANUAL    | BOTH  | OK            | OK

### Pgood
When host is on (pgood == 1), the changes of the above time mode/owner are not
applied but deferred. The changes of the mode/owner are saved to persistent
storage.

When host is off (pgood == 0), the saved mode/owner are read from
persistent storage and are applied.

Note: user can set the time mode and owner in settings daemon at any time,
but time manager applying them is governed by the above condition.
