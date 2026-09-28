#pragma once

#include <SDL2/SDL.h>

#include <atomic>
#include <condition_variable> // was missing, so the build failed outside macOS
#include <mutex>

#include "RingBuffer.h"

// State shared between the decoder thread, the SDL audio callback and the renderer.
struct AudioData {
    static constexpr int kVisualSamples = 4096;

    Uint8 *buf = nullptr; // the whole WAV file, as 16-bit samples
    Uint32 len = 0;       // size of buf in bytes

    // Latest samples played, copied for the renderer.
    float audio_samples[kVisualSamples] = {};
    int sample_count = 0;
    std::mutex audio_mutex;

    // Decoder -> audio callback.
    RingBuffer ring_buf;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop{false};
};
