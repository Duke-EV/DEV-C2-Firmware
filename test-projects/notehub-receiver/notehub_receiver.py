import csv
import json
import os
import time
from pathlib import Path

import requests


BASE_URL = "https://api.notefile.net/v1/projects"
NOTEFILE = "telemetry_v1.qo"
POLL_SECONDS = 5
PAGE_SIZE = 100

CSV_FIELDS = [
    "event_uid", "session_uid", "device_uid", "captured_time", "received_time",
    "uptime_ms", "peripherals_windshield", "peripherals_back_running_lights",
    "peripherals_turn", "peripherals_headlights", "peripherals_brake_lights",
    "peripherals_hazard", "pdb_current_ma", "pdb_voltage_mv",
    "pdb_motor_enabled", "motor_rpm", "throttle_raw", "throttle_average",
    "joulemeter_current", "joulemeter_voltage", "joulemeter_energy",
]

TELEMETRY_FIELDS = CSV_FIELDS[5:]


class ReceiverError(Exception):
    pass


def load_env_file(path):
    path = Path(path)
    if not path.exists():
        return
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        value = value.strip().strip('"').strip("'")
        os.environ.setdefault(key.strip(), value)


def load_config(directory):
    directory = Path(directory)
    load_env_file(directory / ".env")
    project_uid = os.getenv("PROJECT_UID", "").strip()
    token = os.getenv("NOTEHUB_TOKEN", "").strip()
    if not project_uid:
        raise ReceiverError("PROJECT_UID is missing from .env")
    if not token:
        raise ReceiverError("NOTEHUB_TOKEN is missing from .env")
    return {
        "project_uid": project_uid,
        "token": token,
        "device_uid": os.getenv("DEVICE_UID", "").strip(),
        "cursor_path": directory / ".notehub_cursor",
        "csv_path": directory / "telemetry.csv",
    }


def event_params(config, **values):
    params = {"files": NOTEFILE, "pageSize": PAGE_SIZE}
    if config["device_uid"]:
        params["deviceUID"] = config["device_uid"]
    params.update(values)
    return params


def events_url(config):
    return f"{BASE_URL}/{config['project_uid']}/events"


def request_json(session, config, params, sleep_fn=time.sleep):
    delay = POLL_SECONDS
    headers = {"Authorization": f"Bearer {config['token']}"}
    while True:
        try:
            response = session.get(
                events_url(config), headers=headers, params=params, timeout=10
            )
        except requests.RequestException as error:
            print(f"Notehub connection failed: {error}; retrying in {delay}s")
        else:
            if response.status_code in (401, 403):
                raise ReceiverError(
                    f"Notehub authorization failed with HTTP {response.status_code}"
                )
            if response.status_code < 400:
                try:
                    result = response.json()
                except ValueError:
                    print(f"Notehub returned invalid JSON; retrying in {delay}s")
                else:
                    if isinstance(result, dict):
                        return result
                    print(f"Notehub returned an invalid response; retrying in {delay}s")
            elif response.status_code != 429 and response.status_code < 500:
                raise ReceiverError(f"Notehub request failed with HTTP {response.status_code}")
            else:
                print(f"Notehub HTTP {response.status_code}; retrying in {delay}s")
        sleep_fn(delay)
        delay = min(delay * 2, 60)


def load_cursor(path):
    path = Path(path)
    if not path.exists():
        return None
    cursor = path.read_text(encoding="utf-8").strip()
    return cursor or None


def save_cursor(path, cursor):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(str(cursor), encoding="utf-8")
    temporary.replace(path)


def load_seen_events(path):
    path = Path(path)
    if not path.exists():
        return set()
    with path.open("r", newline="", encoding="utf-8") as handle:
        return {
            row["event_uid"]
            for row in csv.DictReader(handle)
            if row.get("event_uid")
        }


def event_row(event):
    body = event.get("body")
    if not isinstance(body, dict):
        body = {}
    row = {
        "event_uid": event.get("event", ""),
        "session_uid": event.get("session", ""),
        "device_uid": event.get("device", ""),
        "captured_time": event.get("when", ""),
        "received_time": event.get("received", ""),
    }
    for field in TELEMETRY_FIELDS:
        row[field] = body.get(field, "")
    return row


def extract_events(response):
    events = response.get("events")
    if not isinstance(events, list):
        raise ReceiverError("Notehub response is missing the events list")
    return [event for event in events if isinstance(event, dict)]


def write_events(path, events, seen):
    new_events = []
    for event in events:
        event_uid = event.get("event", "")
        if event_uid and event_uid not in seen:
            new_events.append(event)
            seen.add(event_uid)
    if not new_events:
        return 0

    path = Path(path)
    needs_header = not path.exists() or path.stat().st_size == 0
    with path.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS)
        if needs_header:
            writer.writeheader()
        for event in new_events:
            writer.writerow(event_row(event))
            print(json.dumps(event, indent=2, sort_keys=True))
    return len(new_events)


def fetch_latest_batch(session, config):
    starting_cursor = str(time.time_ns() // 1000)
    recent = request_json(
        session,
        config,
        event_params(config, pageSize=1, sortBy="captured", sortOrder="desc"),
    )
    recent_events = extract_events(recent)
    if not recent_events:
        return [], starting_cursor

    session_uid = recent_events[0].get("session", "")
    if not session_uid:
        return recent_events, starting_cursor

    events = []
    page_number = 1
    while True:
        response = request_json(
            session,
            config,
            event_params(
                config,
                sessionUID=session_uid,
                pageNum=page_number,
                sortBy="captured",
                sortOrder="asc",
            ),
        )
        page = extract_events(response)
        events.extend(page)
        if len(page) < PAGE_SIZE:
            break
        page_number += 1
    events.sort(key=lambda event: (event.get("received", 0), event.get("when", 0)))
    return events, starting_cursor


def poll_new_events(session, config, cursor, seen):
    while True:
        response = request_json(
            session, config, event_params(config, cursor=cursor)
        )
        write_events(config["csv_path"], extract_events(response), seen)
        next_cursor = response.get("next_cursor")
        if not next_cursor:
            raise ReceiverError("Notehub cursor response is missing next_cursor")
        if str(next_cursor) == str(cursor) and response.get("has_more"):
            raise ReceiverError("Notehub cursor did not advance")
        cursor = str(next_cursor)
        save_cursor(config["cursor_path"], cursor)
        if not response.get("has_more", False):
            return cursor


def run_receiver(config):
    seen = load_seen_events(config["csv_path"])
    cursor = load_cursor(config["cursor_path"])
    with requests.Session() as session:
        if cursor is None:
            print("No saved cursor; loading the latest uploaded telemetry batch")
            events, cursor = fetch_latest_batch(session, config)
            write_events(config["csv_path"], events, seen)
        else:
            print("Resuming from the saved Notehub cursor")

        while True:
            cursor = poll_new_events(session, config, cursor, seen)
            time.sleep(POLL_SECONDS)


def main():
    directory = Path(__file__).resolve().parent
    try:
        config = load_config(directory)
        run_receiver(config)
    except KeyboardInterrupt:
        print("\nNotehub receiver stopped")
        return 0
    except ReceiverError as error:
        print(f"Receiver error: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
