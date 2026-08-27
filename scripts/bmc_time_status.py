#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# bmc_time_status.py
#
# Diagnostic tool for phosphor-time-manager.
# Displays:
#   - Current BMC time          (via D-Bus EpochTime.Elapsed)
#   - Time sync mode            (NTP or Manual)
#   - Last time change          (from journal TIME_CHANGE_AUDIT entries)
#   - NTP server + sync status  (when mode is NTP, via timedatectl D-Bus)
#
# Usage (on a live BMC):
#   python3 bmc_time_status.py
#   python3 bmc_time_status.py --json
#   python3 bmc_time_status.py --history 5
#
# Requirements:
#   - busctl + journalctl  (always present on OpenBMC — part of systemd)
#   - python3-dbus         (optional — busctl fallback used if absent)
#   - python3-systemd      (optional — journalctl fallback used if absent)

import argparse
import datetime
import json
import re
import subprocess
import sys

DBUS_TIME_SERVICE     = "xyz.openbmc_project.Time.Manager"
DBUS_TIME_PATH        = "/xyz/openbmc_project/time/bmc"
DBUS_TIME_IFACE       = "xyz.openbmc_project.Time.EpochTime"

DBUS_SETTINGS_SERVICE = "xyz.openbmc_project.Settings"
DBUS_SETTINGS_PATH    = "/xyz/openbmc_project/time/sync_method"
DBUS_SETTINGS_IFACE   = "xyz.openbmc_project.Time.Synchronization"
DBUS_SETTINGS_PROP    = "TimeSyncMethod"

DBUS_TIMEDATE_SERVICE = "org.freedesktop.timedate1"
DBUS_TIMEDATE_PATH    = "/org/freedesktop/timedate1"
DBUS_TIMEDATE_IFACE   = "org.freedesktop.timedate1"

DBUS_PROPS_IFACE      = "org.freedesktop.DBus.Properties"

NTP_MODE_STRING       = "xyz.openbmc_project.Time.Synchronization.Method.NTP"
MANUAL_MODE_STRING    = "xyz.openbmc_project.Time.Synchronization.Method.Manual"
AUDIT_MARKER          = "TIME_CHANGE_AUDIT"


def _short_mode(full_mode_string: str) -> str:
    if full_mode_string == NTP_MODE_STRING:
        return "NTP"
    if full_mode_string == MANUAL_MODE_STRING:
        return "Manual"
    return full_mode_string


def _epoch_us_to_utc(epoch_us: int) -> str:
    dt = datetime.datetime.fromtimestamp(
        epoch_us / 1_000_000, tz=datetime.timezone.utc
    )
    return dt.strftime("%Y-%m-%d %H:%M:%S UTC")


def _run(cmd: list[str]) -> str:
    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True, timeout=5
        )
        return result.stdout.strip()
    except Exception:
        return ""


def _busctl_get(service: str, path: str, iface: str, prop: str) -> str:
    raw = _run(["busctl", "--system", "get-property",
                service, path, iface, prop])
    if not raw:
        raise RuntimeError(
            f"busctl get-property returned empty for "
            f"{service} {path} {iface} {prop}"
        )
    parts = raw.split(" ", 1)
    return parts[1].strip().strip('"') if len(parts) == 2 else raw.strip()


def get_current_time() -> dict:
    epoch_us = None
    try:
        import dbus
        bus      = dbus.SystemBus()
        obj      = bus.get_object(DBUS_TIME_SERVICE, DBUS_TIME_PATH)
        props    = dbus.Interface(obj, DBUS_PROPS_IFACE)
        epoch_us = int(props.Get(DBUS_TIME_IFACE, "Elapsed"))
    except ImportError:
        pass
    except Exception as exc:
        raise RuntimeError(f"Failed to read current time from D-Bus: {exc}")

    if epoch_us is None:
        epoch_us = int(_busctl_get(DBUS_TIME_SERVICE, DBUS_TIME_PATH,
                                   DBUS_TIME_IFACE, "Elapsed"))
    return {
        "epoch_us":   epoch_us,
        "utc_string": _epoch_us_to_utc(epoch_us),
    }


def get_sync_mode() -> dict:
    full = None
    try:
        import dbus
        bus   = dbus.SystemBus()
        obj   = bus.get_object(DBUS_SETTINGS_SERVICE, DBUS_SETTINGS_PATH)
        props = dbus.Interface(obj, DBUS_PROPS_IFACE)
        full  = str(props.Get(DBUS_SETTINGS_IFACE, DBUS_SETTINGS_PROP))
    except ImportError:
        pass
    except Exception as exc:
        raise RuntimeError(f"Failed to read sync mode from D-Bus: {exc}")

    if full is None:
        full = _busctl_get(DBUS_SETTINGS_SERVICE, DBUS_SETTINGS_PATH,
                           DBUS_SETTINGS_IFACE, DBUS_SETTINGS_PROP)
    return {"full": full, "short": _short_mode(full)}


def get_ntp_status() -> dict:
    active = None
    synced = None
    try:
        import dbus
        bus    = dbus.SystemBus()
        obj    = bus.get_object(DBUS_TIMEDATE_SERVICE, DBUS_TIMEDATE_PATH)
        props  = dbus.Interface(obj, DBUS_PROPS_IFACE)
        active = bool(props.Get(DBUS_TIMEDATE_IFACE, "NTP"))
        synced = bool(props.Get(DBUS_TIMEDATE_IFACE, "NTPSynchronized"))
    except ImportError:
        pass
    except Exception as exc:
        raise RuntimeError(f"Failed to read NTP status from D-Bus: {exc}")

    if active is None:
        active = _busctl_get(DBUS_TIMEDATE_SERVICE, DBUS_TIMEDATE_PATH,
                             DBUS_TIMEDATE_IFACE, "NTP") == "true"
        synced = _busctl_get(DBUS_TIMEDATE_SERVICE, DBUS_TIMEDATE_PATH,
                             DBUS_TIMEDATE_IFACE,
                             "NTPSynchronized") == "true"

    server = ""
    raw = _run(["timedatectl", "show-timesync", "--all"])
    for line in raw.splitlines():
        if line.startswith("ServerName=") or \
           line.startswith("SystemNTPServer="):
            server = line.split("=", 1)[1].strip()
            break

    if not server:
        raw = _run(["chronyc", "tracking"])
        m = re.search(r"Reference ID\s+:\s+\S+\s+\((.+?)\)", raw)
        if m:
            server = m.group(1).strip()

    return {
        "ntp_active":       active,
        "ntp_synchronized": synced,
        "server":           server,
    }


def get_audit_history(max_entries: int = 1) -> list[dict]:
    entries = []
    try:
        from systemd import journal
        reader = journal.Reader()
        reader.log_level(journal.LOG_INFO)
        reader.add_match(
            _SYSTEMD_UNIT="xyz.openbmc_project.Time.Manager.service"
        )
        reader.seek_tail()
        collected = 0
        while collected < max_entries:
            try:
                entry = reader.get_previous()
            except StopIteration:
                break
            msg = entry.get("MESSAGE", "")
            if AUDIT_MARKER not in msg:
                continue
            ts = entry.get("__REALTIME_TIMESTAMP")
            entries.append({
                "changed_at":    _epoch_us_to_utc(int(ts)) if ts
                                 else "unknown",
                "previous_time": entry.get("PREVIOUS_TIME", ""),
                "new_time":      entry.get("NEW_TIME", ""),
                "delta_seconds": entry.get("DELTA_SECONDS", ""),
                "mode":          _short_mode(entry.get("MODE", "")),
                "raw_message":   msg,
            })
            collected += 1
        return entries
    except ImportError:
        pass
    except Exception as exc:
        return [{"error": str(exc)}]

    raw = _run([
        "journalctl",
        "-u", "xyz.openbmc_project.Time.Manager",
        "--output=json", "--no-pager",
        "-n", str(max_entries * 20),
    ])
    for line in raw.splitlines():
        if not line.strip():
            continue
        try:
            j   = json.loads(line)
            msg = j.get("MESSAGE", "")
            if AUDIT_MARKER not in msg:
                continue
            ts = j.get("__REALTIME_TIMESTAMP")
            entries.append({
                "changed_at":    _epoch_us_to_utc(int(ts)) if ts
                                 else "unknown",
                "previous_time": j.get("PREVIOUS_TIME", ""),
                "new_time":      j.get("NEW_TIME", ""),
                "delta_seconds": j.get("DELTA_SECONDS", ""),
                "mode":          _short_mode(j.get("MODE", "")),
                "raw_message":   msg,
            })
            if len(entries) >= max_entries:
                break
        except (json.JSONDecodeError, KeyError, ValueError):
            continue
    return entries


def print_report(current: dict, mode: dict,
                 ntp: dict | None, history: list[dict]) -> None:
    sep  = "─" * 60
    sep2 = "=" * 60

    print(sep2)
    print("  BMC Time Status Report")
    now_str = (datetime.datetime.now(datetime.timezone.utc)
               .strftime("%Y-%m-%d %H:%M:%S UTC"))
    print(f"  Generated : {now_str}")
    print(sep2)

    print("\n[ Current BMC Time ]")
    print(f"  Time (UTC)  : {current['utc_string']}")
    print(f"  Epoch (µs)  : {current['epoch_us']}")

    print("\n[ Time Sync Mode ]")
    print(f"  Mode        : {mode['short']}")
    print(f"  D-Bus value : {mode['full']}")

    if mode["short"] == "NTP":
        print("\n[ NTP Status ]")
        if ntp:
            active_str = "enabled" if ntp["ntp_active"]       else "disabled"
            sync_str   = "yes ✓"   if ntp["ntp_synchronized"] else "no  ✗"
            print(f"  NTP active  : {active_str}")
            print(f"  Synchronized: {sync_str}")
            print(f"  NTP server  : {ntp['server'] or '(not yet acquired)'}")
            if not ntp["ntp_synchronized"]:
                print("  ⚠ WARNING   : NTP mode is active but clock is "
                      "NOT yet synchronized")
        else:
            print("  (NTP status unavailable)")
    else:
        print("\n[ NTP Status ]")
        print("  N/A — sync mode is Manual")

    print("\n[ Last Time Change ]")
    print(sep)
    if not history:
        print("  No TIME_CHANGE_AUDIT entries found in the journal.")
        print("  (Time has never been set via phosphor-time-manager, or")
        print("   the journal has been rotated.)")
    else:
        last = history[0]
        if "error" in last:
            print(f"  Error reading journal: {last['error']}")
        else:
            print(f"  Changed at    : {last['changed_at']}")
            print(f"  Previous time : {last['previous_time']}")
            print(f"  New time      : {last['new_time']}")
            delta = last.get("delta_seconds", "")
            if delta:
                try:
                    d    = int(delta)
                    sign = "+" if d >= 0 else ""
                    print(f"  Delta         : {sign}{d} seconds")
                    if abs(d) > 3600:
                        print(f"  ⚠ WARNING     : Large time step detected"
                              f" ({sign}{d}s)")
                except ValueError:
                    print(f"  Delta         : {delta}s")
            print(f"  Mode at change: {last['mode']}")

    if len(history) > 1:
        print(f"\n[ Time Change History — last {len(history)} changes ]")
        print(sep)
        for i, rec in enumerate(history, start=1):
            if "error" in rec:
                print(f"  [{i}] Error: {rec['error']}")
            else:
                print(f"  [{i}] {rec['changed_at']}"
                      f"  {rec['previous_time']} → {rec['new_time']}"
                      f"  Δ{rec.get('delta_seconds', '?')}s"
                      f"  ({rec['mode']})")
    print(sep2)


def print_json_report(current: dict, mode: dict,
                      ntp: dict | None, history: list[dict]) -> None:
    print(json.dumps({
        "current_time":  current,
        "sync_mode":     mode,
        "ntp_status":    ntp,
        "audit_history": history,
    }, indent=2, default=str))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="BMC time status diagnostic tool for phosphor-time-manager"
    )
    parser.add_argument("--json",    action="store_true",
                        help="Output as JSON instead of human-readable text")
    parser.add_argument("--history", type=int, default=1, metavar="N",
                        help="Number of past time-change events to show "
                             "(default: 1)")
    args   = parser.parse_args()
    errors = []

    try:
        current = get_current_time()
    except RuntimeError as e:
        current = {"epoch_us": 0, "utc_string": f"ERROR: {e}"}
        errors.append(str(e))

    try:
        mode = get_sync_mode()
    except RuntimeError as e:
        mode = {"full": f"ERROR: {e}", "short": "unknown"}
        errors.append(str(e))

    ntp = None
    if mode["short"] == "NTP":
        try:
            ntp = get_ntp_status()
        except RuntimeError as e:
            ntp = {
                "ntp_active":       False,
                "ntp_synchronized": False,
                "server":           f"ERROR: {e}",
            }
            errors.append(str(e))

    history = get_audit_history(max_entries=max(args.history, 1))

    if args.json:
        print_json_report(current, mode, ntp, history)
    else:
        print_report(current, mode, ntp, history)

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
