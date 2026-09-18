#include "SDL/SailfishSensors.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <atomic>

#include <QAccelerometer>
#include <QCoreApplication>
#include <QOrientationSensor>
#include <QThread>

#include <SDL.h>

#include "Common/Log.h"
#include "Common/System/Display.h"
#include "Common/System/NativeApp.h"
#include "Common/GPU/thin3d.h"
#include "Core/Config.h"
#include "Core/ConfigValues.h"

extern DisplayRotation g_forcedDisplayRotation;

namespace SailfishSensors {
namespace {
QCoreApplication *g_app;
QAccelerometer *g_accel;
QOrientationSensor *g_orient;
bool g_active;
int g_lastOrientation = -1;
bool g_wholeApp;
void (*g_resizeCb)();

constexpr float kPerG = 1.0f / 9.80665f;
std::atomic<int> g_accelThreadState{0};  // 0 starting, 1 running, 2 failed
int g_accelThreadRate;

// The accelerometer on its own thread with its own event loop: every sample
// reaches the emulated stick at the sensor's rate. Polled from the render
// loop instead, a sample waited for the next rendered frame and then for the
// next sceCtrl sampling - about 26 ms on average before the game saw it.
class AccelThread : public QThread {
	void run() override {
		QAccelerometer acc;
		acc.setAccelerationMode(QAccelerometer::Combined);
		if (!acc.connectToBackend()) { g_accelThreadState = 2; return; }
		int rate = 0;
		for (const qrange &r : acc.availableDataRates())
			if (r.second > rate) rate = r.second;
		if (rate <= 0) rate = 100;
		acc.setDataRate(rate);
		QObject::connect(&acc, &QSensor::readingChanged, [&acc]() {
			if (QAccelerometerReading *r = acc.reading())
				NativeAccelerometer(r->x() * kPerG, r->y() * kPerG, r->z() * kPerG);
		});
		if (!acc.start()) { g_accelThreadState = 2; return; }
		g_accelThreadRate = acc.dataRate() > 0 ? acc.dataRate() : rate;
		g_accelThreadState = 1;
		exec();
		acc.stop();
	}
};
AccelThread *g_accelThread;

// Which of the two landscape rotations goes with which device pose depends on
// how the rotation matrices are defined; TiltAutoRotateSwap flips it.
DisplayRotation RotationFor(int reading) {
	bool rightUp = reading == QOrientationReading::RightUp;
	if (g_Config.bTiltAutoRotateSwap) rightUp = !rightUp;
	return rightUp ? DisplayRotation::ROTATE_90 : DisplayRotation::ROTATE_270;
}

// The rotation matrices were written for Vulkan's clip space (y down). OpenGL's
// clip space is y up, so the same matrix turns the picture the other way round
// there -- 180 degrees away from where RotateRectToDisplay() puts the scissor
// and viewport rects, which are written for Vulkan's direction. On GL the
// opposite matrix lands the picture where the rects are.
void SetRotMatrix(DisplayRotation want) {
	bool gl = g_Config.iGPUBackend == (int)GPUBackend::OPENGL;
	g_display.rot_matrix.setIdentity();
	if (want == DisplayRotation::ROTATE_90) { if (gl) g_display.rot_matrix.setRotationZ270(); else g_display.rot_matrix.setRotationZ90(); }
	else if (want == DisplayRotation::ROTATE_270) { if (gl) g_display.rot_matrix.setRotationZ90(); else g_display.rot_matrix.setRotationZ270(); }
	else if (want == DisplayRotation::ROTATE_180) g_display.rot_matrix.setRotationZ180();
}

void SetRotation(DisplayRotation want) {
	if (want == g_forcedDisplayRotation) return;
	g_forcedDisplayRotation = want;
	g_display.rotation = want;
	SetRotMatrix(want);
	// Deliberately no content-orientation hint here: we turn the picture
	// ourselves. Told "landscape", Lipstick configures the surface as
	// 2272x1032 and rescales our portrait buffer into it - that was the
	// picture squashed into a band.
	fprintf(stderr, "[sailfish] display rotation -> %d\n", (int)want);
	if (g_resizeCb) g_resizeCb();
}

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
const char *OrientationName(int r) {
	switch (r) {
	case QOrientationReading::TopUp: return "TopUp";
	case QOrientationReading::TopDown: return "TopDown";
	case QOrientationReading::LeftUp: return "LeftUp";
	case QOrientationReading::RightUp: return "RightUp";
	case QOrientationReading::FaceUp: return "FaceUp";
	case QOrientationReading::FaceDown: return "FaceDown";
	default: return "Undefined";
	}
}

void ApplyOrientation(int reading) {
	int cur = g_Config.iInternalScreenRotation;
	fprintf(stderr, "[sailfish] orientation now %s (%d), internal rotation %d, whole-app %d\n", OrientationName(reading), reading, cur, g_wholeApp);
	if (g_wholeApp) {
		if (reading == QOrientationReading::LeftUp || reading == QOrientationReading::RightUp)
			SetRotation(RotationFor(reading));
		return;
	}
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
		fprintf(stderr, "[sailfish] device orientation %d -> internal rotation %d\n", reading, want);
		g_Config.iInternalScreenRotation = want;
	}
}
}  // namespace

bool Init() {
	if (g_active) return true;
	if (getenv("PPSSPP_NO_SENSORS")) return false;
	EnsureApp();
	// Preferred: the accelerometer thread. PPSSPP_ACCEL_POLL=1 forces the old
	// per-frame polling.
	if (!getenv("PPSSPP_ACCEL_POLL")) {
		g_accelThread = new AccelThread();
		g_accelThread->start();
		for (int i = 0; i < 200 && g_accelThreadState == 0; ++i)
			QThread::msleep(10);
		if (g_accelThreadState == 1) {
			INFO_LOG(Log::System, "Sailfish: accelerometer thread at %d Hz", g_accelThreadRate);
			fprintf(stderr, "[sailfish] accelerometer thread running at %d Hz\n", g_accelThreadRate);
			g_active = true;
		} else {
			WARN_LOG(Log::System, "Sailfish: accelerometer thread did not start, polling instead");
			g_accelThread->quit();
			g_accelThread->wait();
			delete g_accelThread;
			g_accelThread = nullptr;
		}
	}
	if (!g_accelThread) {
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
	}

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
	static int polls = 0;
	++polls;
	QCoreApplication::processEvents();
	static bool logIt = getenv("PPSSPP_SENSOR_LOG") != nullptr;
	QAccelerometerReading *r = g_accel ? g_accel->reading() : nullptr;
	if (logIt && (polls == 1 || polls % 200 == 0))
		fprintf(stderr, "[sailfish] poll #%d, %s, orientation reading %s\n", polls,
		        g_accelThread ? "accelerometer on its own thread" : (r ? "reading yes" : "reading null"),
		        g_orient && g_orient->reading() ? "yes" : "null");
	if (r) {
		// Qt reports m/s^2 in the device's portrait frame (x right, y up,
		// z out of the screen) -- the Android convention NativeAccelerometer
		// expects. Handed on in g so the NFS Shift path's 0.15 g validity
		// threshold means what it did in the original.
		const float k = 1.0f / 9.80665f;
		NativeAccelerometer(r->x() * k, r->y() * k, r->z() * k);
		// PPSSPP_SENSOR_LOG=1: one line per second with the raw vector, for
		// checking axes and signs on a device one cannot hold oneself.
		static int n = 0;
		if (logIt && (++n % 50) == 0)
			fprintf(stderr, "[sailfish] accel g = %.2f %.2f %.2f  rotation %d\n", r->x() * k, r->y() * k, r->z() * k, g_Config.iInternalScreenRotation);
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

bool WholeAppRotation() { return g_wholeApp; }

void PrepareWindow() {
	// Only the Vulkan backend knows how to render pre-rotated (scissors,
	// backbuffer size); with OpenGL we keep the old scheme, where only the
	// game picture is turned by InternalScreenRotation.
	// Vulkan renders pre-rotated through VulkanContext, OpenGL through the
	// viewport/scissor rotation in GLQueueRunner. PPSSPP_NO_ROTATE=1 returns
	// to the old scheme (portrait UI, only the game picture turned).
	g_wholeApp = !getenv("PPSSPP_NO_ROTATE");
	if (!g_wholeApp) return;
	// Start on the side the device is already held, else the usual one.
	DisplayRotation start = DisplayRotation::ROTATE_90;
	if (g_orient) {
		QCoreApplication::processEvents();
		if (QOrientationReading *o = g_orient->reading()) {
			int v = (int)o->orientation();
			if (v == QOrientationReading::LeftUp || v == QOrientationReading::RightUp) start = RotationFor(v);
		}
	}
	g_forcedDisplayRotation = start;
	g_display.rotation = start;
	SetRotMatrix(start);
	// We pre-rotate the whole picture ourselves, so Lipstick must treat the
	// surface as plain portrait (geometry 1032x2272 = the EGL drawable). With
	// "landscape" it reshapes the surface to 2272x1032 and rescales our buffer
	// into it (picture squashed into a band); that state even lingers for the
	// app across relaunches, hence set "portrait" explicitly every start.
	SDL_SetHint(SDL_HINT_QTWAYLAND_CONTENT_ORIENTATION, "portrait");
	// The whole picture is turned now; a game rotated on top of that would be
	// turned twice.
	g_Config.iInternalScreenRotation = ROTATION_LOCKED_HORIZONTAL;
	fprintf(stderr, "[sailfish] whole-app landscape, start rotation %d\n", (int)start);
}

bool Rotated() {
	return g_wholeApp && (g_forcedDisplayRotation == DisplayRotation::ROTATE_90 || g_forcedDisplayRotation == DisplayRotation::ROTATE_270);
}

void RotateTouch(float &x, float &y, float physW, float physH) {
	if (!g_wholeApp) return;
	// Inverse of RotateRectToDisplay() in Common/System/Display.cpp.
	float px = x, py = y;
	switch (g_forcedDisplayRotation) {
	case DisplayRotation::ROTATE_90:  x = py; y = physW - px; break;
	case DisplayRotation::ROTATE_270: x = physH - py; y = px; break;
	case DisplayRotation::ROTATE_180: x = physW - px; y = physH - py; break;
	default: break;
	}
}

void SetResizeCallback(void (*cb)()) { g_resizeCb = cb; }

void Shutdown() {
	if (g_accelThread) {
		g_accelThread->quit();
		g_accelThread->wait();
	}
	if (g_accel) g_accel->stop();
	if (g_orient) g_orient->stop();
	g_active = false;
}
}  // namespace SailfishSensors
