Name:       ppsspp
Summary:    PPSSPP PSP Emulator for Sailfish OS
Version:    1.19.0
Release:    17
Group:      Applications/Games
License:    GPLv2+
URL:        https://www.ppsspp.org/
Requires:   SDL2, libGLESv2, libEGL

%description
A fast and portable PSP emulator optimized for mobile platforms.

%prep
# Keine Vorbereitung nötig

%build
# Bereits im SB2-Kontext gebaut

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/ppsspp
mkdir -p %{buildroot}/usr/share/applications

cp -a /home/mersdk/ppsspp/pkg/usr/bin/ppsspp /home/mersdk/ppsspp/pkg/usr/bin/ppsspp-bin %{buildroot}/usr/bin/
cp -a /home/mersdk/ppsspp/pkg/usr/share/ppsspp/assets %{buildroot}/usr/share/ppsspp/
cp -a /home/mersdk/ppsspp/pkg/usr/share/applications/ppsspp.desktop %{buildroot}/usr/share/applications/

%files
%defattr(-,root,root,-)
/usr/bin/ppsspp
/usr/bin/ppsspp-bin
/usr/share/ppsspp/
/usr/share/applications/ppsspp.desktop

%changelog
* Fri Sep 18 2026 Sebastian Matkovich <sebastian.matkovich@gmail.com> - 1.19.0-17
- Touch layout, sound and tilt settings are device-wide, no longer per-game:
  a change in the main menu applies to every game; per-game inis keep only
  rendering settings (resolution, effects)
- NFS Shift tilt model: "Low end radius" (inverse dead zone) and "NFS Shift
  smoothing" (filter time constant, 111.2 ms = original) for games that
  filter the stick themselves, e.g. Gran Turismo (low end ~0.5)
- Steering direction fixed for the landscape poses (was mirrored)
- Accelerometer on its own thread at the sensor rate (400 Hz on the Jolla C2)
  instead of once per rendered frame
- Launcher pins the emulator to the big cores and holds the big cluster at
  1650 MHz while running (restored on exit): the emulation no longer stutters
  when the scheduler parks the emu thread on little or downclocked cores
- Picture no longer squashed into a band: plain portrait window instead of
  FULLSCREEN_DESKTOP (Lipstick reports the output in its own orientation),
  logical size from the EGL drawable, DPI from the longer sides
- GL pre-rotation: opposite Z rotation on OpenGL (y-up clip space), rotated
  scissors kept through resizes, ROTATE_270 clamp fixed
- Debug: PPSSPP_SENSOR_LOG=1 prints sensor, DPI and window geometry decisions

* Thu Sep 18 2026 Sebastian Matkovich <sebastian.matkovich@gmail.com> - 1.19.0-5
- Sensors from sensorfw through QtSensors, tilt steering as in NFS Shift,
  auto-rotation between the landscape poses, whole-app landscape
