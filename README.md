# Introduction

`phosphor-time-manager` is the time manager service that implements D-Bus
interface `xyz/openbmc_project/Time/EpochTime.interface.yaml`. The user can get
or set the BMC's time via this interface.

## Configuration

phosphor-time-manager is configured by setting `-D` flags that correspond to
options in `phosphor-time-manager/meson.options`. The option names become C++
preprocessor symbols that control which code is compiled into the program.

- Compile phosphor-time-manager with default options:

  ```bash
     meson setup builddir
     ninja -C builddir
  ```

- Compile phosphor-time-manager with some configurable options:

  ```bash
     meson setup builddir -Dbuildtype=minsize  -Dtests=disabled
     ninja -C builddir
  ```

- Generate test coverage report:

  ```bash
     meson setup builddir -Db_coverage=true -Dtests=enabled
     ninja coverage -C builddir test
  ```

### General usage

The service `xyz.openbmc_project.Time.Manager` provides an object on D-Bus:

- /xyz/openbmc_project/time/bmc

where each object implements interface `xyz.openbmc_project.Time.EpochTime`.

The user can directly get or set the property `Elapsed` of the objects to get or
set the time. For example on an authenticated session:

- To get BMC's time:

  ```bash
  ### With busctl on BMC
  busctl get-property xyz.openbmc_project.Time.Manager \
      /xyz/openbmc_project/time/bmc xyz.openbmc_project.Time.EpochTime Elapsed

  ### With REST API on remote host
  curl -b cjar -k https://${BMC_IP}/xyz/openbmc_project/time/bmc
  ```

- To set BMC's time:

  ```bash
  ### With busctl on BMC
  busctl set-property xyz.openbmc_project.Time.Manager \
      /xyz/openbmc_project/time/bmc xyz.openbmc_project.Time.EpochTime \
      Elapsed t <value-in-microseconds>

  ### With REST API on remote host
  curl -b cjar -k -H "Content-Type: application/json" -X PUT \
      -d '{"data": 1487304700000000}' \
      https://${BMC_IP}/xyz/openbmc_project/time/bmc/attr/Elapsed
  ```

### Time settings

Getting BMC time is always allowed, but setting the time may not be allowed
depending on the below two settings in the settings manager.

- TimeSyncMethod
  - NTP: The time is set via NTP server.
  - MANUAL: The time is set manually.

A summary of which cases the time can be set on BMC or HOST:

| Mode   | Set BMC Time |
| ------ | ------------ |
| NTP    | Fail to set  |
| MANUAL | OK           |

- To set an NTP [server](https://tf.nist.gov/tf-cgi/servers.cgi):

  ```bash
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

- To go into NTP mode

  ```bash
  ### With busctl on BMC
  busctl set-property xyz.openbmc_project.Settings \
      /xyz/openbmc_project/time/sync_method xyz.openbmc_project.Time.Synchronization \
      TimeSyncMethod s "xyz.openbmc_project.Time.Synchronization.Method.NTP"

  ### With REST API on remote host
  curl -c cjar -b cjar -k -H "Content-Type: application/json" -X  PUT  -d \
      '{"data": "xyz.openbmc_project.Time.Synchronization.Method.NTP" }' \
      https://${BMC_IP}/xyz/openbmc_project/time/sync_method/attr/TimeSyncMethod
  ```

### Special note on changing NTP setting

Starting from OpenBMC 2.6 (with systemd v239), systemd's timedated introduces a
new beahvior that it checks the NTP services' status during setting time,
instead of checking the NTP setting:

- When NTP server is set to disabled, and the NTP service is stopping but not
  stopped, setting time will get an error.

In OpenBMC 2.4 (with systemd v236), the above will always succeed.

This results in [openbmc/openbmc#3459][1], and the related test cases are
updated to cooperate with this behavior change.

[1]: https://github.com/openbmc/openbmc/issues/3459
