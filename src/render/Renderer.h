// Renderer.h — declares the Renderer class interface.
// main.cpp only needs to know WHAT the renderer can do, not HOW it does it.

#pragma once
#include "audio/AudioData.h"
#include <GL/glew.h>

class Renderer {
  public:
    bool init();                      // sets up shaders, VAO, VBO
    void draw(AudioData *audio_data); // called every frame
    void cleanup();                   // deletes GPU resources
};