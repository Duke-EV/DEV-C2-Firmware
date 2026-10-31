// Prevents this header from being included more than once during one compilation
#pragma once


bool init_notecard_telemetry();
// true when notecard configured right
// false when not

void update_notecard_telemetry();
// updates telemetry
// this will be called in loop
