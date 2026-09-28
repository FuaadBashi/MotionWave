cmake_minimum_required(VERSION 3.20)
project(MotionWave LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

# The ring buffer has no SDL or OpenGL dependency, so it builds and tests on its own.
add_library(motionwave_audio STATIC src/audio/RingBuffer.cpp)
target_include_directories(motionwave_audio PUBLIC ${CMAKE_SOURCE_DIR}/src)

option(MOTIONWAVE_BUILD_APP "Build the SDL2/OpenGL visualiser" ON)
if(MOTIONWAVE_BUILD_APP)
    find_package(SDL2 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)

    add_executable(MotionWave src/main.cpp src/render/Renderer.cpp)
    target_include_directories(MotionWave PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(MotionWave PRIVATE motionwave_audio ${SDL2_LIBRARIES} GLEW::GLEW
                                             OpenGL::GL Threads::Threads)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(ring_buffer_test tests/ring_buffer_test.cpp)
    target_link_libraries(ring_buffer_test PRIVATE motionwave_audio Threads::Threads)
    add_test(NAME ring_buffer_test COMMAND ring_buffer_test)

    add_executable(sample_exchange_test tests/sample_exchange_test.cpp)
    target_include_directories(sample_exchange_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(sample_exchange_test PRIVATE Threads::Threads)
    add_test(NAME sample_exchange_test COMMAND sample_exchange_test)
endif()
