// Renderer.cpp — owns all OpenGL code.
// Nothing in here knows about SDL2 or the window.
// It only speaks to the GPU.

#include <iostream>
#include "Renderer.h"

const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
)";

static unsigned int shaderProgram, vertexID, bufferID;

bool Renderer::init() {
    float points[9] = {
         0.0f,  1.0f, 0.0f,
        -0.866f, -0.5f, 0.0f,
         0.866f, -0.5f, 0.0f
    };

    glGenVertexArrays(1, &vertexID);
    glGenBuffers(1, &bufferID);
    glBindVertexArray(vertexID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::VERTEX SHADER\n" << infoLog << "\n";
        return false;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER PROGRAM\n" << infoLog << "\n";
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void Renderer::draw(AudioData *audio_data) {

    std::lock_guard<std::mutex> lock(audio_data->audio_mutex);
    int size =  sizeof(audio_data->audio_samples) / sizeof(audio_data->audio_samples[0]);
    float vertices[4096 * 3];
    for (int i = 0; i < size; ++i) {
        vertices[i*3] = (i / 4095.0f) * 2.0f - 1.0f;
        vertices[i*3+1] = audio_data->audio_samples[i];
         vertices[i*3+2] = 0;

    }
    
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(vertexID);
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINE_STRIP, 0, size);

    
}

void Renderer::cleanup() {
    glDeleteVertexArrays(1, &vertexID);
    glDeleteBuffers(1, &bufferID);
    glDeleteProgram(shaderProgram);
}