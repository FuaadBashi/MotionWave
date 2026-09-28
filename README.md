# MotionWave

[![CI](https://github.com/FuaadBashi/MotionWave/actions/workflows/ci.yml/badge.svg)](https://github.com/FuaadBashi/MotionWave/actions/workflows/ci.yml)

A real-time audio visualiser in C++20: it plays a WAV file and draws its waveform live with
OpenGL, using SDL2 for the window and audio output.

<p align="center"><img src="docs/screenshot.png" alt="MotionWave drawing a live waveform" width="640"></p>

## How it works

```
decoder thread ──▶ lock-free ring buffer ──▶ SDL audio callback ──▶ speakers
                                                    │
                                                    └─(try-lock, copy)──▶ OpenGL renderer (60 fps)
```

- **Three threads, one direction of data.** A decoder thread converts samples to floats and
  feeds a ring buffer. SDL's real-time audio callback drains it. The render loop draws the most
  recent block.
- **Lock-free ring buffer.** A single-producer, single-consumer ring with atomic read and write
  indices and acquire/release ordering, so the audio callback never waits on the decoder. It is
  tested with a million-sample producer/consumer run under ThreadSanitizer. If the decoder falls
  behind, the callback plays silence for that block (an underrun) rather than stalling.
- **Non-blocking hand-off to the renderer.** The callback only *tries* the lock on the shared
  sample buffer ([`SampleExchange`](src/audio/SampleExchange.h)). If the renderer is mid-copy,
  that block is not drawn and the audio thread does not wait. A unit test holds the buffer from a
  "renderer" thread and checks that publishing returns immediately.
- **Back-pressure.** The decoder sleeps on a condition variable while the buffer is full and is
  woken by the callback, so it never busy-waits and stays at most one buffer ahead of playback.
- **Clean shutdown.** Closing the window sets a stop flag and wakes the decoder, so the app exits
  immediately rather than waiting for the track to finish.
- **Any WAV.** 8-bit, 24-bit and float files are converted to 16-bit with `SDL_AudioCVT` on load.

[`docs/Renderer.annotated.cpp`](docs/Renderer.annotated.cpp) is a line-by-line walkthrough of the
OpenGL side: shaders, VAOs, VBOs and the draw call.

## Build

Requires CMake 3.20+, a C++20 compiler, SDL2 and GLEW.

```bash
# macOS:          brew install cmake sdl2 glew
# Debian/Ubuntu:  sudo apt install cmake libsdl2-dev libglew-dev
git clone https://github.com/FuaadBashi/MotionWave.git
cd MotionWave
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MotionWave path/to/song.wav
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

[MotionWavePlus](https://github.com/FuaadBashi/MotionWavePlus) is the follow-up, rebuilt on
raylib and miniaudio.
