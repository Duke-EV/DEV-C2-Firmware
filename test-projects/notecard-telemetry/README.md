# Notecard Telemetry Host Test

This project compiles the production `notecard_telemetry.cpp` against fake
Arduino, Wire, Notecard, and vehicle interfaces. It does not require ESP32 or
Notecard hardware and does not contact Notehub.

The test verifies initialization request order, template types, button
debouncing, ten-second sampling, every vehicle field, a failed `note.add`, and
the final `hub.sync` request.

Run with PlatformIO:

```powershell
platformio run -d test-projects/notecard-telemetry
```

Run the produced native executable to execute the assertions. On Windows, the
exact executable path depends on the compiler selected by PlatformIO.

The test can also be compiled directly with a C++11 compiler using the build
flags from `platformio.ini`.
