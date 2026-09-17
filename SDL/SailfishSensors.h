// Sailfish OS: accelerometer and device orientation for the SDL front end.
//
// SDL2 on Sailfish has no sensor backend and its Wayland window never turns
// with the device, so both come from QtSensors (sensorfw) here. The
// accelerometer is fed into NativeAccelerometer() -- the same entry point the
// Android build uses -- and the orientation sensor flips the internal screen
// rotation between the two landscape variants.
#pragma once

namespace SailfishSensors {
// Creates the QCoreApplication the sensors need (unless one exists) and
// starts both sensors. Safe to call once; returns false without an
// accelerometer backend.
bool Init();
// Pumps Qt, forwards the latest accelerometer reading and applies
// orientation changes. Call once per main-loop iteration on the main thread.
void Poll();
void Shutdown();
}
