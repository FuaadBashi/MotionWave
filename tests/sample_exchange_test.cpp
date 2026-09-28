#include "audio/SampleExchange.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition);     \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static float out[SampleExchange::kCapacity];

static void a_published_block_is_what_the_renderer_reads() {
    SampleExchange exchange;
    const float block[] = {0.25f, -0.5f, 1.0f};

    CHECK(exchange.tryPublish(block, 3));

    CHECK(exchange.copyLatest(out) == 3);
    CHECK(out[0] == 0.25f && out[1] == -0.5f && out[2] == 1.0f);
}

static void an_oversized_block_is_truncated_to_capacity() {
    static float big[SampleExchange::kCapacity + 100] = {};
    SampleExchange exchange;

    CHECK(exchange.tryPublish(big, SampleExchange::kCapacity + 100));

    CHECK(exchange.copyLatest(out) == SampleExchange::kCapacity);
}

// The README's claim: the audio thread never waits for the renderer. A "renderer" thread holds
// the buffer while another thread publishes; the publish must return at once, unsuccessfully,
// and leave the previous block in place.
static void publishing_does_not_wait_while_the_renderer_holds_the_buffer() {
    SampleExchange exchange;
    const float old_block[] = {1.0f};
    exchange.tryPublish(old_block, 1);

    std::promise<void> reader_inside;
    std::promise<void> release_reader;
    std::future<void> inside = reader_inside.get_future();
    std::shared_future<void> release = release_reader.get_future().share();
    std::thread renderer([&] {
        exchange.read([&](const float *, int) {
            reader_inside.set_value();
            release.wait();
        });
    });
    inside.wait();

    const float new_block[] = {2.0f, 3.0f};
    auto attempt =
        std::async(std::launch::async, [&] { return exchange.tryPublish(new_block, 2); });
    const bool returned_promptly =
        attempt.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    release_reader.set_value();
    renderer.join();

    CHECK(returned_promptly);
    CHECK(!attempt.get());
    CHECK(exchange.copyLatest(out) == 1 && out[0] == 1.0f);
}

int main() {
    a_published_block_is_what_the_renderer_reads();
    an_oversized_block_is_truncated_to_capacity();
    publishing_does_not_wait_while_the_renderer_holds_the_buffer();

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("All tests passed\n");
    return EXIT_SUCCESS;
}
