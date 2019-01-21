# Introduction
`phosphor-time-manager` is the time manager service that implements D-Bus
interface `xyz/openbmc_project/Time/EpochTime.interface.yaml`.
The user can get or set the BMC's or HOST's time via this interface.

### General usage
The service `xyz.openbmc_project.Time.Manager` provides two objects on D-Bus:
* /xyz/openbmc_project/time/bmc
* /xyz/openbmc_project/time/host

where each object implements interface `xyz.openbmc_project.Time.EpochTime`.

The user can directly get or set the property `Elapsed` of the objects to get or set
the time. For example on an authenticated session:

* To get BMC's time:
   ```
   ### With busctl on BMC
   busctl get-property xyz.openbmc_project.Time.Manager \
       /xyz/openbmc_project/time/bmc xyz.openbmc_project.Time.EpochTime Elapsed

   ### With REST API on remote host
   curl -b cjar -k https://${BMC_IP}/xyz/openbmc_project/time/bmc
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
       https://${BMC_IP}/xyz/openbmc_project/time/host/attr/Elapsed
   ```

### Time settings
Getting BMC or HOST time is always allowed, but setting the time may not be
allowed depending on the below two settings in the settings manager.

* TimeSyncMethod
   * NTP: The time is set via NTP server.
   * MANUAL: The time is set manually.
* TimeOwner
   * BMC: BMC owns the time and can set the time.
   * HOST: Host owns the time and can set the time.
   * SPLIT: BMC and Host own separate time.
   * BOTH: Both BMC and Host can set the time.

A summary of which cases the time can be set on BMC or HOST:

Mode      | Owner | Set BMC Time  | Set Host Time
--------- | ----- | ------------- | -------------------
NTP       | BMC   | Fail to set   | Not allowed
NTP       | HOST  | Not allowed   | Not allowed
NTP       | SPLIT | Fail to set   | OK
NTP       | BOTH  | Fail to set   | Not allowed
MANUAL    | BMC   | OK            | Not allowed
MANUAL    | HOST  | Not allowed   | OK
MANUAL    | SPLIT | OK            | OK
MANUAL    | BOTH  | OK            | OK

* To set an NTP [server](https://tf.nist.gov/tf-cgi/servers.cgi):
   ```
   ### With busctl on BMC
   busctl set-property  xyz.openbmc_project.Network \
      /xyz/openbmc_project/network/eth0 \
      xyz.openbmc_project.Network.EthernetInterface NTPServers \
      as 1 "<ntp_server>"

   ### With REST API on remote host
   curl -c cjar -b cjar -k -H "Content-Type: application/json" -X  PUT  -d \
       '{"data": ["<ntp_server>"] }' \
       https://${BMC_IP}/xyz/openbmc_project/network/eth0/attr/NTPServers
   ```

* To go into NTP mode
   ```
   ### With busctl on BMC
   busctl set-property xyz.openbmc_project.Settings \
       /xyz/openbmc_project/time/sync_method xyz.openbmc_project.Time.Synchronization \
       TimeSyncMethod s "xyz.openbmc_project.Time.Synchronization.Method.NTP"

   ### With REST API on remote host
   curl -c cjar -b cjar -k -H "Content-Type: application/json" -X  PUT  -d \
       '{"data": "xyz.openbmc_project.Time.Synchronization.Method.NTP" }' \
       https://${BMC_IP}/xyz/openbmc_project/time/sync_method/attr/TimeSyncMethod
   ```

* To change owner
   ```
   ### With busctl on BMC
   busctl set-property xyz.openbmc_project.Settings \
       /xyz/openbmc_project/time/owner xyz.openbmc_project.Time.Owner \
       TimeOwner s xyz.openbmc_project.Time.Owner.Owners.BMC

   ### With REST API on remote host
   curl -c cjar -b cjar -k -H "Content-Type: application/json" -X  PUT  -d \
       '{"data": "xyz.openbmc_project.Time.Owner.Owners.BMC" }' \
       https://${BMC_IP}/xyz/openbmc_project/time/owner/attr/TimeOwner
   ```

### Special note on changing NTP setting
Starting from OpenBMC 2.6 (with systemd v239), systemd's timedated introduces
a new beahvior that it checks the NTP services' status during setting time,
instead of checking the NTP setting:

1. When NTP server is set to disabled, and the NTP service is stopping but not
   stopped, setting time will get an error.
2. When NTP server is set to enabled, and the NTP service is starting but not
   started, setting time is OK.

Previously, in case 1, setting time is OK, and in case 2, setting time will
get an error.

In [systemd#11420][1], case 2 is fixed that setting time will get an error as
well. But case 1 is expected.

So eventually with systemd's fix, for case 1, setting time will get an error
when NTP services are not fully stopped.

### Special note on host on
When the host is on, the changes of the above time mode/owner are not applied but
deferred. The changes of the mode/owner are saved to persistent storage.

When the host is off, the saved mode/owner are read from persistent storage and are
applied.

Note: A user can set the time mode and owner in the settings daemon at any time,
but the time manager applying them is governed by the above condition.


[1]: https://github.com/systemd/systemd/issues/11420
