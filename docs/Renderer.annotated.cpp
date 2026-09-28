// ============================================================
// Renderer.cpp — owns all OpenGL code.
//
// This file is intentionally isolated from SDL2 and the window.
// Its only job is to talk to the GPU: compile shaders, allocate
// GPU memory, and draw the waveform every frame.
//
// KEY OPENGL CONCEPTS USED HERE:
//
//   Shader      — a small program that runs ON the GPU.
//                 Two types used here:
//                   Vertex shader   — runs once per vertex (point).
//                                     Decides WHERE on screen each point goes.
//                   Fragment shader — runs once per pixel.
//                                     Decides what COLOUR each pixel is.
//
//   VAO (Vertex Array Object)
//               — remembers HOW to interpret the vertex data.
//                 Stores the layout: "each vertex is 3 floats (x, y, z)".
//                 You bind the VAO once and OpenGL knows the format.
//
//   VBO (Vertex Buffer Object)
//               — a chunk of memory ON THE GPU that holds vertex data.
//                 We upload our float array here every frame.
//
//   Shader Program
//               — the vertex shader + fragment shader linked together
//                 into one object the GPU can execute.
// ============================================================

#include <iostream>
#include "Renderer.h"


// ============================================================
// VERTEX SHADER SOURCE (GLSL)
//
// This tiny program runs on the GPU once for EVERY vertex we draw.
// A vertex is just a point in 3D space (x, y, z).
//
// "layout (location = 0) in vec3 aPos"
//   — reads one vertex from our VBO. location=0 matches the
//     slot number we set up in glVertexAttribPointer().
//
// "gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0)"
//   — sets the final screen position of this vertex.
//     vec4 is a 4-component vector — the 4th value (w=1.0) is
//     required by OpenGL's coordinate system (homogeneous coords).
//     Since our x and y are already in clip space [-1, 1],
//     we pass them straight through with no transformation.
//
// R"(...)" is a raw string literal in C++ — lets us write
// multi-line strings without escaping newlines.
// ============================================================
const char* vertexShaderSource = R"(#version 330 core
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";


// ============================================================
// FRAGMENT SHADER SOURCE (GLSL)
//
// This tiny program runs on the GPU once for EVERY PIXEL
// that our geometry covers on screen.
//
// "out vec4 FragColor"
//   — the output: the colour of this pixel.
//     vec4 = (Red, Green, Blue, Alpha), each 0.0 to 1.0.
//
// "FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f)"
//   — sets every pixel to a fixed orange colour.
//     R=1.0, G=0.5, B=0.2, A=1.0 (fully opaque).
//     This is why the waveform is orange.
// ============================================================
const char* fragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
)";


// ============================================================
// STATIC GPU RESOURCE HANDLES
//
// These are unsigned integers (IDs) that OpenGL uses to refer
// to GPU-side objects. Think of them like file descriptors —
// they are just numbers that represent something on the GPU.
//
//   shaderProgram — the compiled+linked vertex+fragment shaders
//   vertexID      — the VAO (remembers vertex layout)
//   bufferID      — the VBO (holds the actual vertex data on GPU)
//
// "static" means these are only visible within this .cpp file.
// They persist for the entire lifetime of the program.
// ============================================================
static unsigned int shaderProgram, vertexID, bufferID;


// ============================================================
// Renderer::init()
//
// Called ONCE at startup. Sets up everything the GPU needs:
//   1. Allocate a VAO and VBO on the GPU
//   2. Upload placeholder vertex data so the VBO exists
//   3. Tell OpenGL the layout of our vertex data
//   4. Compile the vertex and fragment shaders
//   5. Link them into a shader program
//
// Returns true on success, false if any shader fails to compile.
// ============================================================
bool Renderer::init() {

    // Placeholder triangle vertices — 3 points, each with x/y/z.
    // These get replaced every frame in draw() with real audio data,
    // but we need SOMETHING here so the VBO is created with a size.
    float points[9] = {
         0.0f,  1.0f, 0.0f,
        -0.866f, -0.5f, 0.0f,
         0.866f, -0.5f, 0.0f
    };

    // --- CREATE VAO ---
    // glGenVertexArrays() asks OpenGL to create 1 VAO and store its ID in vertexID.
    // The VAO will remember our vertex layout (3 floats per vertex).
    glGenVertexArrays(1, &vertexID);

    // --- CREATE VBO ---
    // glGenBuffers() asks OpenGL to create 1 buffer object on the GPU.
    // bufferID is the handle we use to refer to it later.
    glGenBuffers(1, &bufferID);

    // Bind the VAO first — everything we set up next gets remembered by it.
    glBindVertexArray(vertexID);

    // Bind the VBO to the GL_ARRAY_BUFFER slot.
    // GL_ARRAY_BUFFER is the slot for vertex data buffers.
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);

    // Upload the placeholder triangle data to the GPU.
    // GL_STATIC_DRAW hints that this data won't change often
    // (we override this in draw() where we use GL_DYNAMIC_DRAW).
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    // Tell OpenGL how to interpret the raw bytes in the VBO:
    //   location 0    — matches "layout (location = 0)" in the vertex shader
    //   3             — each vertex has 3 components (x, y, z)
    //   GL_FLOAT      — each component is a 32-bit float
    //   GL_FALSE      — do not normalise the values
    //   3*sizeof(float) — stride: how many bytes to skip to get to the next vertex
    //   (void*)0      — offset: vertex data starts at byte 0 of the VBO
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Enable vertex attribute slot 0 (disabled by default).
    // Without this, the vertex shader would not receive any data.
    glEnableVertexAttribArray(0);


    // --- COMPILE VERTEX SHADER ---
    // glCreateShader() creates an empty shader object on the GPU.
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

    // glShaderSource() uploads our GLSL source code string into the shader object.
    // The "1" means we are passing 1 string; NULL means strings are null-terminated.
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);

    // glCompileShader() compiles the GLSL source into GPU machine code.
    glCompileShader(vertexShader);


    // --- COMPILE FRAGMENT SHADER ---
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);


    // --- CHECK FOR COMPILE ERRORS ---
    // glGetShaderiv() queries the compile status of the vertex shader.
    // If success == 0, compilation failed and we print the error log.
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::VERTEX SHADER\n" << infoLog << "\n";
        return false;
    }


    // --- LINK SHADERS INTO A PROGRAM ---
    // A shader program combines the vertex and fragment shaders.
    // The GPU runs them in sequence: vertex shader first, then fragment shader.
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check that linking succeeded.
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER PROGRAM\n" << infoLog << "\n";
        return false;
    }

    // The individual shader objects are no longer needed now that they
    // are linked into the program. Free their GPU memory.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}


// ============================================================
// Renderer::draw()
//
// Called EVERY FRAME (~60 times per second) from the render loop.
// Reads the latest audio samples, builds a waveform as vertices,
// uploads them to the GPU, and draws them as a line.
//
// WAVEFORM MAPPING:
//   Each audio sample becomes one vertex.
//   X position = where we are in the sample array, mapped to [-1, 1]
//   Y position = the sample value, already in [-1, 1]
//   Z position = 0 (flat, no depth needed)
//
// OpenGL clip space goes from -1 to 1 in both X and Y,
// which maps perfectly to our audio sample range.
// ============================================================
void Renderer::draw(AudioData* audio_data) {

    // Lock the mutex before reading audio_samples[].
    // The audio callback (on SDL2's thread) writes to audio_samples[]
    // at the same time we are trying to read it here.
    // The lock_guard ensures only one thread accesses it at a time.
    // It automatically unlocks when this function returns.
    std::lock_guard<std::mutex> lock(audio_data->audio_mutex);

    // How many samples are in the audio_samples array.
    // sizeof gives bytes; dividing by element size gives count (4096).
    int size = sizeof(audio_data->audio_samples) / sizeof(audio_data->audio_samples[0]);

    // CPU-side vertex buffer: 4096 vertices, each with 3 floats (x, y, z).
    // Total: 4096 * 3 = 12288 floats. Built fresh every frame.
    float vertices[4096 * 3];

    // Build the vertex array from the audio samples.
    for (int i = 0; i < size; ++i) {

        // X: map sample index i to the range [-1.0, 1.0].
        // Formula: (i / 4095.0) gives [0, 1], * 2 - 1 shifts to [-1, 1].
        // i=0    -> x = -1.0 (left edge of screen)
        // i=4095 -> x =  1.0 (right edge of screen)
        vertices[i * 3] = (i / 4095.0f) * 2.0f - 1.0f;

        // Y: the audio sample value, already in [-1.0, 1.0].
        // Loud sounds push the line up/down; silence stays at 0 (centre).
        vertices[i * 3 + 1] = audio_data->audio_samples[i];

        // Z: always 0. We are drawing in 2D so no depth is needed.
        vertices[i * 3 + 2] = 0.0f;
    }

    // --- CLEAR THE SCREEN ---
    // Set the background colour (dark navy: R=0.05, G=0.05, B=0.15).
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
    // Actually clear the colour buffer — wipes the previous frame.
    glClear(GL_COLOR_BUFFER_BIT);

    // --- ACTIVATE THE SHADER PROGRAM ---
    // Tell OpenGL to use our compiled shaders for the next draw call.
    glUseProgram(shaderProgram);

    // --- BIND THE VAO ---
    // Restores the vertex layout we set up in init().
    // OpenGL now knows each vertex is 3 floats.
    glBindVertexArray(vertexID);

    // --- UPLOAD NEW VERTEX DATA TO THE GPU ---
    // Bind the VBO so the next glBufferData call targets it.
    glBindBuffer(GL_ARRAY_BUFFER, bufferID);

    // Upload the freshly built vertices array to the GPU.
    // GL_DYNAMIC_DRAW tells the GPU this data changes every frame —
    // it uses this hint to optimise memory placement for frequent updates.
    // sizeof(vertices) = 4096 * 3 * 4 bytes = 49152 bytes total.
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // --- DRAW THE WAVEFORM ---
    // GL_LINE_STRIP connects each vertex to the next with a line segment,
    // producing one continuous line across the screen.
    // 'size' (4096) tells OpenGL how many vertices to draw.
    glDrawArrays(GL_LINE_STRIP, 0, size);
}


// ============================================================
// Renderer::cleanup()
//
// Called ONCE at shutdown. Frees all GPU resources we allocated.
// Forgetting this leaks GPU memory — always clean up OpenGL objects.
// ============================================================
void Renderer::cleanup() {
    glDeleteVertexArrays(1, &vertexID);  // free the VAO
    glDeleteBuffers(1, &bufferID);       // free the VBO
    glDeleteProgram(shaderProgram);      // free the shader program
}