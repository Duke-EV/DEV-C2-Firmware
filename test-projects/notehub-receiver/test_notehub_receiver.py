import csv
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import requests

import notehub_receiver as receiver


class FakeResponse:
    def __init__(self, status_code, payload=None, invalid_json=False):
        self.status_code = status_code
        self.payload = payload
        self.invalid_json = invalid_json

    def json(self):
        if self.invalid_json:
            raise ValueError("invalid JSON")
        return self.payload


class FakeSession:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []

    def get(self, url, headers, params, timeout):
        self.calls.append({
            "url": url, "headers": headers, "params": params, "timeout": timeout
        })
        response = self.responses.pop(0)
        if isinstance(response, Exception):
            raise response
        return response


def make_config(directory):
    return {
        "project_uid": "app:test",
        "token": "secret-token",
        "device_uid": "dev:test",
        "cursor_path": Path(directory) / ".notehub_cursor",
        "csv_path": Path(directory) / "telemetry.csv",
    }


def make_event(event_uid, session_uid="session:test", rpm=1234):
    return {
        "event": event_uid,
        "session": session_uid,
        "device": "dev:test",
        "when": 100,
        "received": 101,
        "body": {"motor_rpm": rpm, "pdb_current_ma": 40000},
    }


class ReceiverTests(unittest.TestCase):
    def test_request_retries_and_sends_authentication(self):
        session = FakeSession([
            requests.Timeout("timeout"),
            FakeResponse(500, {}),
            FakeResponse(200, {"events": []}),
        ])
        sleeps = []
        config = make_config(".")
        result = receiver.request_json(
            session, config, {"files": receiver.NOTEFILE}, sleeps.append
        )
        self.assertEqual(result, {"events": []})
        self.assertEqual(sleeps, [5, 10])
        self.assertEqual(
            session.calls[-1]["headers"]["Authorization"], "Bearer secret-token"
        )

    def test_authorization_failure_is_fatal(self):
        session = FakeSession([FakeResponse(401, {})])
        with self.assertRaises(receiver.ReceiverError):
            receiver.request_json(session, make_config("."), {}, lambda delay: None)

    def test_csv_mapping_and_deduplication(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "telemetry.csv"
            seen = set()
            event = make_event("event:1", rpm=5678)
            self.assertEqual(receiver.write_events(path, [event, event], seen), 1)
            self.assertEqual(receiver.write_events(path, [event], seen), 0)
            with path.open(newline="", encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]["motor_rpm"], "5678")
            self.assertEqual(rows[0]["peripherals_turn"], "")
            self.assertEqual(receiver.load_seen_events(path), {"event:1"})

    def test_cursor_round_trip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".notehub_cursor"
            self.assertIsNone(receiver.load_cursor(path))
            receiver.save_cursor(path, "12345.67")
            self.assertEqual(receiver.load_cursor(path), "12345.67")

    def test_first_run_loads_latest_session(self):
        newest = make_event("event:2", "session:new")
        older = make_event("event:1", "session:new")
        older["received"] = 99
        session = FakeSession([
            FakeResponse(200, {"events": [newest]}),
            FakeResponse(200, {"events": [newest, older]}),
        ])
        events, cursor = receiver.fetch_latest_batch(session, make_config("."))
        self.assertEqual([event["event"] for event in events], ["event:1", "event:2"])
        self.assertTrue(cursor.isdigit())
        self.assertEqual(session.calls[0]["params"]["pageSize"], 1)
        self.assertEqual(session.calls[1]["params"]["sessionUID"], "session:new")

    def test_cursor_poll_drains_pages_and_saves_progress(self):
        with tempfile.TemporaryDirectory() as directory:
            config = make_config(directory)
            session = FakeSession([
                FakeResponse(200, {
                    "events": [make_event("event:1")],
                    "next_cursor": "cursor:2", "has_more": True,
                }),
                FakeResponse(200, {
                    "events": [make_event("event:2")],
                    "next_cursor": "cursor:3", "has_more": False,
                }),
            ])
            result = receiver.poll_new_events(session, config, "cursor:1", set())
            self.assertEqual(result, "cursor:3")
            self.assertEqual(receiver.load_cursor(config["cursor_path"]), "cursor:3")
            self.assertEqual(session.calls[1]["params"]["cursor"], "cursor:2")

    def test_rate_limit_and_invalid_json_retry(self):
        session = FakeSession([
            FakeResponse(429, {}),
            FakeResponse(200, invalid_json=True),
            FakeResponse(200, {"events": []}),
        ])
        sleeps = []
        result = receiver.request_json(session, make_config("."), {}, sleeps.append)
        self.assertEqual(result, {"events": []})
        self.assertEqual(sleeps, [5, 10])

    def test_malformed_event_response_is_rejected(self):
        with self.assertRaises(receiver.ReceiverError):
            receiver.extract_events({"unexpected": []})

    def test_local_env_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, ".env").write_text(
                "PROJECT_UID=app:local\nNOTEHUB_TOKEN=local-token\n",
                encoding="utf-8",
            )
            with patch.dict(os.environ, {}, clear=True):
                config = receiver.load_config(directory)
            self.assertEqual(config["project_uid"], "app:local")
            self.assertEqual(config["token"], "local-token")

    def test_event_parameters_include_filters(self):
        params = receiver.event_params(make_config("."), cursor="cursor:1")
        self.assertEqual(params["files"], "telemetry_v1.qo")
        self.assertEqual(params["deviceUID"], "dev:test")
        self.assertEqual(params["cursor"], "cursor:1")


if __name__ == "__main__":
    unittest.main()
