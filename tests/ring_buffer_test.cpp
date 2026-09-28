// Tests for the lock-free ring buffer shared by the decoder thread and the audio callback.

#include "audio/RingBuffer.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition);     \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void samples_come_out_in_the_order_they_went_in() {
    RingBuffer rb;
    const float in[] = {0.1f, 0.2f, 0.3f};
    float out[3] = {};

    CHECK(rb.write(in, 3));
    CHECK(rb.size() == 3);
    CHECK(rb.read(out, 3));
    CHECK(out[0] == 0.1f && out[1] == 0.2f && out[2] == 0.3f);
    CHECK(rb.size() == 0);
}

static void a_read_larger_than_the_buffered_samples_fails_without_consuming_any() {
    RingBuffer rb;
    const float in[] = {1.0f, 2.0f};
    float out[3] = {};
    rb.write(in, 2);

    CHECK(!rb.read(out, 3));
    CHECK(rb.size() == 2);
}

static void a_write_that_would_overflow_is_refused() {
    RingBuffer rb;
    std::vector<float> chunk(RingBuffer::kCapacity, 0.5f);

    CHECK(rb.write(chunk.data(), chunk.size()));
    CHECK(!rb.has_space(1));
    CHECK(!rb.write(chunk.data(), 1));
}

static void indices_wrap_around_the_end_of_the_storage() {
    RingBuffer rb;
    std::vector<float> chunk(RingBuffer::kCapacity - 10, 0.0f);
    std::vector<float> sink(chunk.size());
    rb.write(chunk.data(), chunk.size());
    rb.read(sink.data(), sink.size());

    std::vector<float> in(100), out(100);
    for (int i = 0; i < 100; ++i) {
        in[i] = static_cast<float>(i);
    }
    CHECK(rb.write(in.data(), in.size())); // straddles the end of the array
    CHECK(rb.read(out.data(), out.size()));
    CHECK(out == in);
}

static void a_producer_and_consumer_thread_exchange_every_sample_in_order() {
    RingBuffer rb;
    constexpr int kTotal = 1'000'000;
    constexpr int kChunk = 512;

    std::thread producer([&] {
        float chunk[kChunk];
        for (int sent = 0; sent < kTotal; sent += kChunk) {
            for (int i = 0; i < kChunk; ++i) {
                chunk[i] = static_cast<float>(sent + i);
            }
            while (!rb.write(chunk, kChunk)) {
                std::this_thread::yield();
            }
        }
    });

    bool in_order = true;
    float chunk[kChunk];
    for (int received = 0; received < kTotal; received += kChunk) {
        while (!rb.read(chunk, kChunk)) {
            std::this_thread::yield();
        }
        for (int i = 0; i < kChunk; ++i) {
            in_order = in_order && chunk[i] == static_cast<float>(received + i);
        }
    }
    producer.join();

    CHECK(in_order);
}

int main() {
    samples_come_out_in_the_order_they_went_in();
    a_read_larger_than_the_buffered_samples_fails_without_consuming_any();
    a_write_that_would_overflow_is_refused();
    indices_wrap_around_the_end_of_the_storage();
    a_producer_and_consumer_thread_exchange_every_sample_in_order();

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("All tests passed\n");
    return EXIT_SUCCESS;
}
