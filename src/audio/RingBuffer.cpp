#include "RingBuffer.h"

std::size_t RingBuffer::size() const {
    return write_pos_.load(std::memory_order_acquire) - read_pos_.load(std::memory_order_acquire);
}

bool RingBuffer::has_space(std::size_t count) const {
    return kCapacity - size() >= count;
}

bool RingBuffer::write(const float *data, std::size_t count) {
    if (!has_space(count)) {
        return false;
    }
    std::size_t pos = write_pos_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < count; ++i) {
        buffer_[(pos + i) & kMask] = data[i];
    }
    // Publish the samples only after they are written.
    write_pos_.store(pos + count, std::memory_order_release);
    return true;
}

bool RingBuffer::read(float *dest, std::size_t count) {
    if (size() < count) {
        return false;
    }
    std::size_t pos = read_pos_.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < count; ++i) {
        dest[i] = buffer_[(pos + i) & kMask];
    }
    read_pos_.store(pos + count, std::memory_order_release);
    return true;
}
