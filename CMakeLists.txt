cmake_minimum_required(VERSION 3.27)
project(DancoreEngine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW (submodule)
add_subdirectory(third_party/glfw)

# ImGui (build as static lib with backends)
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

# Our headers
include_directories(${CMAKE_SOURCE_DIR})

# Editor app (added in subdir)
add_subdirectory(apps/Editor)