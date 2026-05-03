#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include "render/Renderer.h"
#include "audio/AudioData.h" 
#include <mutex>



void audioCallback(void* userdata, Uint8* stream, int len) {
    AudioData* audio = (AudioData*)userdata;
    std::lock_guard<std::mutex> lock(audio->audio_mutex); 
       
    if (audio->pos >= audio->len) {
        SDL_memset(stream, 0, len);
        return;
    }
    
    Uint32 remaining = audio->len - audio->pos;
    Uint32 toCopy = (len > (int)remaining) ? remaining : len;
    
    SDL_memcpy(stream, audio->buf + audio->pos, toCopy);

    // Sint16* stream16 = (Sint16*)stream;
    // Uint32 max_samples = sizeof(audio->audio_samples) / sizeof(audio->audio_samples[0]);
    // Uint32 iteration_length = ((int) toCopy/2 >max_samples) ? max_samples : toCopy/2;

    // for (int i = 0; i < iteration_length; ++i){
    //     audio->audio_samples[i] = stream16[i]/32768.0f;
    // }  -------- OLD improved version below -------

    Sint16* samples = reinterpret_cast<Sint16*>(stream);

    // bytes → samples (16-bit = 2 bytes per sample)
    Uint32 samples_requested = toCopy / sizeof(Sint16);

    // derive buffer capacity safely
    Uint32 max_samples = sizeof(audio->audio_samples) / sizeof(audio->audio_samples[0]);
    Uint32 samples_to_process = (samples_requested > max_samples) ? max_samples : samples_requested;

    // convert to float [-1, 1]
    for (Uint32 i = 0; i < samples_to_process; ++i) {
        audio->audio_samples[i] = samples[i] / 32768.0f;
    }
    audio->sample_count = samples_to_process;
    audio->pos += toCopy;
    
    if (toCopy < (Uint32)len)
        SDL_memset(stream + toCopy, 0, len - toCopy);
    
} // i used ai to help for this function

int main(int argc, char* argv[]) {

    // --- SDL2 INIT ---
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cout << SDL_GetError();
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // --- WINDOW ---
    SDL_Window* window = SDL_CreateWindow(
        "VLC_Motion",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    if (!window) { std::cout << SDL_GetError(); return -1; }

    // --- OPENGL CONTEXT ---
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) { std::cout << SDL_GetError(); return -1; }

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
    if (!renderer.init()) return -1;
    std::cout << "Renderer init done\n";    

    // --- AUDIO FILE ---
    const char* file = "/Users/fuaadshurie/Desktop/cable-car.wav";
    SDL_AudioSpec spec;
    Uint8* audio_buf;
    Uint32 audio_len;

    SDL_LoadWAV(file, &spec, &audio_buf, &audio_len);
    if (!audio_buf) {
        std::cout << "SDL_LoadWAV failed: " << SDL_GetError() << "\n";
        return -1;
    }

    // AudioData audioData = { audio_buf, audio_len, 0 };
    AudioData audio_data;
    audio_data.buf = audio_buf;
    audio_data.len = audio_len;
    audio_data.pos = 0;
    audio_data.sample_count = 0;
    spec.callback = audioCallback;
    spec.userdata = &audio_data;


    std::cout << "WAV loaded, buf=" << (void*)audio_buf << " len=" << audio_len << "\n";

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
            if (event.type == SDL_QUIT) running = false;
        }
        renderer.draw(&audio_data);
        SDL_GL_SwapWindow(window);
    }

    // --- CLEANUP ---
    SDL_CloseAudioDevice(OpenAudioDevice);
    SDL_FreeWAV(audio_buf);
    renderer.cleanup();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}