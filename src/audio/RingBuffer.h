#pragma once
#include <atomic>

class RingBuffer {
    float buffer[16384];
    std::atomic<size_t> write_pos;
    std::atomic<size_t> read_pos;
public:
    bool write(const float* data, size_t count);;
    bool read(float* dest, size_t count);
};
