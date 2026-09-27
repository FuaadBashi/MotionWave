# MotionWave — SDL/OpenGL Audio Visualization

A C++20 audio-visualization prototype using SDL2 for windowing/audio, GLEW/OpenGL for rendering, and a ring buffer between decoding and playback.

## Build

Requires CMake 3.20+, a C++20 compiler, and SDL2 and GLEW discoverable by CMake, plus an OpenGL-capable desktop.

```bash
git clone https://github.com/FuaadBashi/MotionWave.git
cd MotionWave
cmake -S . -B build-local -DCMAKE_BUILD_TYPE=Release
cmake --build build-local
```

## Audio setup and launch

Before building, replace the hard-coded audio path in [src/main.cpp](src/main.cpp) with a local WAV file. Audio files are not bundled.

```bash
./build-local/MotionWavePlus
```

The executable is named `MotionWavePlus` in this repository's CMake file, even though the repository is named MotionWave.

## Code to explore

- [src/main.cpp](src/main.cpp): initialization, render loop, and shutdown.
- [src/audio](src/audio): ring buffer, shared state, and audio support.
- [src/render](src/render): visualization rendering.
- [CMakeLists.txt](CMakeLists.txt): dependencies and target definition.

This is a desktop prototype. Audio initialization, callback bounds, and shutdown behavior need further validation; no real-time safety or performance benchmark is claimed.
