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

// Whole-app landscape (Vulkan backend only): the window stays portrait, PPSSPP
// renders everything pre-rotated and turns the touch input to match. Call
// PrepareWindow() after the config is loaded and before the window exists.
bool WholeAppRotation();
void PrepareWindow();
// True while the whole UI is rendered rotated (dp is landscape, pixels stay
// portrait). SDLMain swaps dp_xres/dp_yres itself after UpdateScreenScale.
bool Rotated();
// Physical touch position (in the same units as physW/physH) -> logical.
void RotateTouch(float &x, float &y, float physW, float physH);
// Called after the rotation changed so the front end re-applies its sizes.
void SetResizeCallback(void (*cb)());
}
