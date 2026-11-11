# Notehub Telemetry Receiver

This program retrieves `telemetry_v1.qo` events from Notehub, prints them, and
appends them to `telemetry.csv`. It runs on a computer, not on the ESP32 or
Notecard.

## Setup

1. Create a Notehub Personal Access Token with access to the project.
2. Create a Python virtual environment and install dependencies:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

3. Copy `.env.example` to `.env` and set:

```text
PROJECT_UID=app:your-project-uid
NOTEHUB_TOKEN=your-personal-access-token
```

`DEVICE_UID` is optional. When omitted, the receiver accepts telemetry from
every device in the project. Never commit `.env` or share the access token.

## Run

```powershell
python notehub_receiver.py
```

On its first run, the receiver prints and saves the newest uploaded Notehub
session, then switches to cursor polling every five seconds. Later runs resume
from `.notehub_cursor`. Delete that file only when you intentionally want to
bootstrap from the latest session again.

## Test

Tests use fake HTTP responses and do not contact Notehub:

```powershell
python -m unittest -v test_notehub_receiver.py
```
