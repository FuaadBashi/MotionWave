cmake_minimum_required(VERSION 3.20)
project(MotionWavePlus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)

add_executable(MotionWavePlus 
    src/main.cpp
    src/render/Renderer.cpp
    src/audio/RingBuffer.cpp
)

target_include_directories(MotionWavePlus PRIVATE ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(MotionWavePlus PRIVATE ${SDL2_LIBRARIES} GLEW::GLEW)

