#pragma once

#include <atomic>
#include <cstddef>

// Single-producer, single-consumer ring buffer of audio samples.
//
// The decoder thread writes and the audio callback reads. Each index is only ever advanced by
// one side, so atomics are enough and the real-time audio callback never waits on a lock.
class RingBuffer {
  public:
    static constexpr std::size_t kCapacity = 16384; // must be a power of two

    bool has_space(std::size_t count) const;
    std::size_t size() const;
    bool write(const float *data, std::size_t count);
    bool read(float *dest, std::size_t count);

  private:
    static_assert((kCapacity & (kCapacity - 1)) == 0, "capacity must be a power of two");
    static constexpr std::size_t kMask = kCapacity - 1;

    float buffer_[kCapacity] = {};
    std::atomic<std::size_t> write_pos_{0};
    std::atomic<std::size_t> read_pos_{0};
};
