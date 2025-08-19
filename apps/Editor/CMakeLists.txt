add_executable(dancore_editor
    main.cpp
    ${CMAKE_SOURCE_DIR}/engine/ui/editor/EditorUI.cpp
)

target_include_directories(dancore_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(dancore_editor PRIVATE
    imgui
    glfw
    Vulkan::Vulkan
)

if(APPLE)
    target_link_libraries(dancore_editor PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()
