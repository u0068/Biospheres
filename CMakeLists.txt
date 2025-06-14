cmake_minimum_required(VERSION 3.20)
project(Biospheres2 VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/Libraries/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}
)

# Link directories
link_directories(${CMAKE_SOURCE_DIR}/Libraries/lib)

# Source files
set(SOURCES
    main.cpp
    glad.c
    camera.cpp
    cell_manager.cpp
    glad_helpers.cpp
    glfw_helpers.cpp
    imgui_helpers.cpp
    input.cpp
    shader_class.cpp
    sphere_mesh.cpp
    timer.cpp
    ui_manager.cpp
    
    # ImGui source files
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_opengl3.cpp
)

# Header files (for IDE)
set(HEADERS
    camera.h
    cell_manager.h
    config.h
    glad_helpers.h
    glfw_helpers.h
    imgui_helpers.h
    input.h
    resource.h
    shader_class.h
    sphere_mesh.h
    timer.h
    ui_manager.h
    
    # ImGui headers
    imgui/imgui.h
    imgui/imgui_impl_glfw.h
    imgui/imgui_impl_opengl3.h
    imgui/imgui_internal.h
    imgui/imconfig.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${OPENGL_LIBRARIES}
    glfw3
    opengl32
)

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Copy shaders to build directory
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR}/bin)

# Set startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
