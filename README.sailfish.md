# PPSSPP for Sailfish OS

A port of [PPSSPP](https://www.ppsspp.org/) (base: v1.18.1) to Sailfish OS,
developed and tested on the Jolla C2 (MediaTek mt6858, Mali-G610, 1032x2272
panel). SDL2 on Wayland, OpenGL ES through libhybris.

What the port adds on top of upstream:

* **Whole-app landscape** on a portrait device through pre-rotation on the
  OpenGL backend (`GLQueueRunner` rotates the backbuffer viewport/scissor,
  `thin3d_gl` swaps the target size, `SetRotMatrix()` uses the opposite Z
  rotation on GL because its clip space is y-up). The window is a plain
  1032x2272 surface - not `SDL_WINDOW_FULLSCREEN_DESKTOP`, whose size Lipstick
  reports in its own orientation - and the logical size comes from the EGL
  drawable (`SDL/SDLMain.cpp`, `ApplySailfishScreen`).
* **Sensors from sensorfw via QtSensors** (`SDL/SailfishSensors.cpp`): the
  accelerometer runs on its own thread at the sensor's rate (400 Hz on the
  C2), the orientation sensor switches between the two landscape poses.
* **NFS Shift steering model** for tilt (`Core/TiltEventProcessor.cpp`,
  `ProcessTiltNfsShift`): the accelerometer-to-steer curve of Need for Speed
  Shift (2010), reverse engineered from the game - angle from `acos`, first
  order filter with a 111.2 ms time constant, full lock at 30 degrees. Two
  extra knobs for games that filter the stick themselves (Gran Turismo):
  *Low end radius* (inverse dead zone, GT needs about 0.5) and *NFS Shift
  smoothing* (0 = only the game's own filter).
* Launcher (`pkg/usr/bin/ppsspp`): pins the emulator to the big cores and
  holds the big cluster at 1650 MHz while running (via passwordless sudo,
  restored on exit) - the scheduler otherwise parks the emulation thread on
  little/downclocked cores and the emulation stutters, audibly in the sound.

## Building

Inside the Sailfish SDK (scratchbox2 target `SailfishOS-5.2.0.15-aarch64`):

    cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSING_GLES2=ON -DUSING_EGL=ON \
          -DUSE_SYSTEM_LIBSDL2=ON -DSAILFISH=ON
    sb2 -t SailfishOS-5.2.0.15-aarch64 sh -c "cd build && make -j8 PPSSPPSDL"
    sh pkg/prepare-assets.sh
    cp build/PPSSPPSDL pkg/usr/bin/ppsspp-bin
    sb2 -t SailfishOS-5.2.0.15-aarch64 rpmbuild -bb ppsspp.spec

## Settings worth knowing

* Per-game inis (`~/.config/ppsspp/PSP/SYSTEM/<GAMEID>_ppsspp.ini`) override
  the global one for sound, touch layout, tilt and resolution - change them
  from inside the game (pause menu), or the change lands in the global file
  and the game ignores it.
* `AutoRun = False` means "start the CPU paused for debugging" - the game
  boots to a black screen.
* Rendering resolution: 4x already saturates the GPU headroom on the C2 in
  Gran Turismo; 8x drops to 35 FPS. `InflightFrames = 1` lowers input
  latency but couples the emulation to every GPU hiccup - leave it at 3.
* `SkipBufferEffects` must stay off: the non-buffered GL path does not apply
  the pre-rotation.
