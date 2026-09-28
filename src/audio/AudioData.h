#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <condition_variable> // was missing, so the build failed outside macOS
#include <mutex>

#include "RingBuffer.h"
#include "SampleExchange.h"

// State shared between the decoder thread, the SDL audio callback and the renderer.
struct AudioData {
    Uint8 *buf = nullptr; // the whole WAV file, as 16-bit samples
    Uint32 len = 0;       // size of buf in bytes

    // Latest samples played, handed to the renderer without the audio callback ever waiting.
    SampleExchange visual;

    // Decoder -> audio callback.
    RingBuffer ring_buf;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};
