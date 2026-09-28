#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <mutex>

// Hands the most recently played block of samples from the audio thread to the render loop.
//
// The audio side only try-locks. A real-time callback that waits on a lock the renderer holds can
// miss its deadline and drop out; here, if the renderer is mid-copy, that block is skipped instead.
// The next callback publishes a fresh one a few milliseconds later.
class SampleExchange {
  public:
    static constexpr int kCapacity = 4096;

    // Audio thread. Never waits: returns false, publishing nothing, if the reader holds the buffer.
    bool tryPublish(const float *samples, int count) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock()) {
            return false;
        }
        count_ = std::clamp(count, 0, kCapacity);
        std::memcpy(samples_, samples, static_cast<std::size_t>(count_) * sizeof(float));
        return true;
    }

    // Render thread. Calls use(samples, count) while holding the buffer, so keep it short.
    template <typename Use> void read(Use &&use) {
        std::lock_guard<std::mutex> lock(mutex_);
        use(static_cast<const float *>(samples_), count_);
    }

    // Render thread. Copies the latest block into out, which must hold kCapacity floats.
    int copyLatest(float *out) {
        int count = 0;
        read([&](const float *samples, int n) {
            std::memcpy(out, samples, static_cast<std::size_t>(n) * sizeof(float));
            count = n;
        });
        return count;
    }

  private:
    std::mutex mutex_;
    float samples_[kCapacity] = {};
    int count_ = 0;
};
