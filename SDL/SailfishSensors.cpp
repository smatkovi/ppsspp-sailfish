#include "SDL/SailfishSensors.h"

#include <cstdlib>

#include <QAccelerometer>
#include <QCoreApplication>
#include <QOrientationSensor>

#include "Common/Log.h"
#include "Common/System/NativeApp.h"
#include "Core/Config.h"
#include "Core/ConfigValues.h"

namespace SailfishSensors {
namespace {
QCoreApplication *g_app;
QAccelerometer *g_accel;
QOrientationSensor *g_orient;
bool g_active;
int g_lastOrientation = -1;

void EnsureApp() {
	if (QCoreApplication::instance()) return;
	static int argc = 1;
	static char name[] = "ppsspp";
	static char *argv[] = { name, nullptr };
	g_app = new QCoreApplication(argc, argv);
}

// The landscape variants of the internal rotation, in the order the device
// reports them: QOrientationReading::RightUp puts the device's top edge to
// the left. Which PPSSPP rotation that corresponds to depends on how PPSSPP
// rotates its output; TiltAutoRotateSwap turns it round if the picture ends
// up upside down.
void ApplyOrientation(int reading) {
	int cur = g_Config.iInternalScreenRotation;
	// Only when the user has chosen one of the two vertical (landscape on a
	// portrait window) rotations: those two are what "auto-rotate" toggles.
	if (cur != ROTATION_LOCKED_VERTICAL && cur != ROTATION_LOCKED_VERTICAL180) return;
	int want;
	switch (reading) {
	case QOrientationReading::LeftUp:  want = ROTATION_LOCKED_VERTICAL; break;
	case QOrientationReading::RightUp: want = ROTATION_LOCKED_VERTICAL180; break;
	default: return;  // portrait, face up/down: keep what we have
	}
	if (g_Config.bTiltAutoRotateSwap)
		want = (want == ROTATION_LOCKED_VERTICAL) ? ROTATION_LOCKED_VERTICAL180 : ROTATION_LOCKED_VERTICAL;
	if (want != cur) {
		INFO_LOG(Log::System, "Sailfish: device orientation %d -> internal rotation %d", reading, want);
		g_Config.iInternalScreenRotation = want;
	}
}
}  // namespace

bool Init() {
	if (g_active) return true;
	if (getenv("PPSSPP_NO_SENSORS")) return false;
	EnsureApp();
	g_accel = new QAccelerometer();
	g_accel->setAccelerationMode(QAccelerometer::Combined);
	if (!g_accel->connectToBackend()) {
		WARN_LOG(Log::System, "Sailfish: no accelerometer backend");
		delete g_accel;
		g_accel = nullptr;
		return false;
	}
	g_accel->setDataRate(50);
	g_active = g_accel->start();
	INFO_LOG(Log::System, "Sailfish: accelerometer %s", g_active ? "started" : "failed to start");

	g_orient = new QOrientationSensor();
	if (g_orient->connectToBackend() && g_orient->start()) {
		INFO_LOG(Log::System, "Sailfish: orientation sensor started");
	} else {
		WARN_LOG(Log::System, "Sailfish: no orientation sensor");
		delete g_orient;
		g_orient = nullptr;
	}
	return g_active;
}

void Poll() {
	if (!g_active) return;
	QCoreApplication::processEvents();
	if (QAccelerometerReading *r = g_accel->reading()) {
		// Qt reports m/s^2 in the device's portrait frame (x right, y up,
		// z out of the screen) -- the Android convention NativeAccelerometer
		// expects. Handed on in g so the NFS Shift path's 0.15 g validity
		// threshold means what it did in the original.
		const float k = 1.0f / 9.80665f;
		NativeAccelerometer(r->x() * k, r->y() * k, r->z() * k);
	}
	if (g_orient) {
		if (QOrientationReading *o = g_orient->reading()) {
			int v = (int)o->orientation();
			if (v != g_lastOrientation) {
				g_lastOrientation = v;
				ApplyOrientation(v);
			}
		}
	}
}

void Shutdown() {
	if (g_accel) g_accel->stop();
	if (g_orient) g_orient->stop();
	g_active = false;
}
}  // namespace SailfishSensors
