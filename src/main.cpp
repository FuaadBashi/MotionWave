#include "audio/AudioData.h"
#include "render/Renderer.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Feeds the ring buffer from the loaded WAV, blocking while the buffer is full.
void decoderThread(AudioData *audio_data) {
    constexpr std::size_t kChunk = 4096;
    float data[kChunk];
    const Sint16 *samples = reinterpret_cast<const Sint16 *>(audio_data->buf);
    const std::size_t total = audio_data->len / sizeof(Sint16);

    for (std::size_t pos = 0; pos < total && !audio_data->stop; pos += kChunk) {
        for (std::size_t j = 0; j < kChunk; ++j) {
            data[j] = pos + j < total ? samples[pos + j] / 32768.0f : 0.0f;
        }

        std::unique_lock<std::mutex> lock(audio_data->mtx);
        // Also wake on stop: otherwise closing the window waited for the whole track to play.
        audio_data->cv.wait(
            lock, [&] { return audio_data->stop || audio_data->ring_buf.has_space(kChunk); });
        if (audio_data->stop) {
            return;
        }
        audio_data->ring_buf.write(data, kChunk);
    }
}

void audioCallback(void *userdata, Uint8 *stream, int len) {
    AudioData *audio = (AudioData *)userdata;

    size_t count = len / sizeof(Sint16);
    // A variable-length array here was a GCC extension that MSVC rejects. The buffer is sized
    // once, on the audio thread, and reused.
    thread_local std::vector<float> temp;
    temp.resize(count);

    bool ring_read = audio->ring_buf.read(temp.data(), count);
    audio->cv.notify_one();

    // On underrun, play silence and publish nothing, so the renderer never draws garbage.
    if (!ring_read) {
        SDL_memset(stream, 0, len);
        return;
    }

    // Never waits: if the renderer is mid-copy, this block is simply not drawn.
    audio->visual.tryPublish(temp.data(), static_cast<int>(count));

    // Convert floats to Sint16 for SDL output
    Sint16 *stream16 = reinterpret_cast<Sint16 *>(stream);
    for (size_t i = 0; i < count; ++i) {
        stream16[i] = (Sint16)(temp[i] * 32767.0f);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.wav>\n";
        return 1;
    }
    const char *file = argv[1];

    // --- SDL2 INIT ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cout << SDL_GetError();
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // --- WINDOW ---
    SDL_Window *window =
        SDL_CreateWindow("MotionWave", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << SDL_GetError();
        return -1;
    }

    // --- OPENGL CONTEXT ---
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cout << SDL_GetError();
        return -1;
    }
    SDL_GL_SetSwapInterval(1);

    // --- GLEW ---
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cout << "GLEW Error: " << glewGetErrorString(err) << "\n";
        return -1;
    }
    std::cout << "GLEW: " << glewGetString(GLEW_VERSION) << "\n";

    // --- RENDERER ---
    std::cout << "Starting renderer init\n";
    Renderer renderer;
    if (!renderer.init())
        return -1;
    std::cout << "Renderer init done\n";

    // --- AUDIO FILE ---
    SDL_AudioSpec spec;
    Uint8 *audio_buf = nullptr;
    Uint32 audio_len = 0;

    if (!SDL_LoadWAV(file, &spec, &audio_buf, &audio_len)) {
        std::cerr << "Could not load " << file << ": " << SDL_GetError() << "\n";
        return 1;
    }

    // The pipeline works in 16-bit samples. Convert 8-bit, 24-bit or float WAVs rather than
    // reinterpreting their bytes as noise.
    if (spec.format != AUDIO_S16SYS) {
        SDL_AudioCVT cvt;
        SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, spec.channels,
                          spec.freq);
        cvt.len = static_cast<int>(audio_len);
        cvt.buf = static_cast<Uint8 *>(SDL_malloc(cvt.len * cvt.len_mult));
        SDL_memcpy(cvt.buf, audio_buf, audio_len);
        SDL_FreeWAV(audio_buf);
        if (SDL_ConvertAudio(&cvt) != 0) {
            std::cerr << "Could not convert audio: " << SDL_GetError() << "\n";
            SDL_free(cvt.buf);
            return 1;
        }
        audio_buf = cvt.buf;
        audio_len = static_cast<Uint32>(cvt.len_cvt);
        spec.format = AUDIO_S16SYS;
    }

    AudioData audio_data;
    audio_data.buf = audio_buf;
    audio_data.len = audio_len;
    std::thread decoder(decoderThread, &audio_data);
    spec.callback = audioCallback;
    spec.userdata = &audio_data;

    std::cout << "WAV loaded, buf=" << (void *)audio_buf << " len=" << audio_len << "\n";

    SDL_AudioSpec obtained;
    int isCapture = 0;
    SDL_AudioDeviceID OpenAudioDevice = SDL_OpenAudioDevice(NULL, isCapture, &spec, &obtained, 0);
    std::cout << "Audio device opened: " << OpenAudioDevice << "\n";
    if (OpenAudioDevice == 0) {
        std::cout << "SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        return -1;
    }
    SDL_PauseAudioDevice(OpenAudioDevice, 0);
    std::cout << "Audio unpaused\n";

    // --- RENDER LOOP ---
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }
        renderer.draw(&audio_data);
        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }

    // --- CLEANUP ---
    audio_data.stop = true;
    audio_data.cv.notify_all();
    decoder.join();
    SDL_CloseAudioDevice(OpenAudioDevice);
    SDL_free(audio_buf); // SDL_FreeWAV is SDL_free, and also covers a converted buffer
    renderer.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}