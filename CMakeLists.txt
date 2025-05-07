cmake_minimum_required(VERSION 3.30)
project(Sinestesia)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH
        "${CMAKE_PREFIX_PATH};/opt/homebrew/opt/glad/lib/cmake"
        "${CMAKE_PREFIX_PATH};/opt/homebrew/opt/glfw/lib/cmake"
        "${CMAKE_PREFIX_PATH};/opt/homebrew/opt/glew/lib/cmake"
)
# Find libraries
find_package(GLFW3 REQUIRED)
find_package(GLEW REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(Sinestesia
        main.cpp
        ${CMAKE_SOURCE_DIR}/lib/serialib.h
        ${CMAKE_SOURCE_DIR}/lib/serialib.cpp
        /Users/tacode/libs/glad/include/glad/glad.c
)

target_include_directories(Sinestesia PRIVATE
        ${GLEW_INCLUDE_DIRS}
        /opt/homebrew/opt/glfw/include
        /Users/tacode/libs/glad/include
        /opt/homebrew/include   # for GLM
)
target_link_libraries(Sinestesia
        GLEW::GLEW
        glfw
)


# Optional: message outputs
message(STATUS "GLFW3_FOUND: ${GLFW3_FOUND}")
message(STATUS "GLEW_FOUND: ${GLEW_FOUND}")

