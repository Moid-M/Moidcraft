cmake_minimum_required(VERSION 3.20)
project(MoidCraft VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")

option(ENABLE_VALIDATION_LAYERS "Enable Vulkan validation layers" OFF)

set(EXTERN_DIR ${CMAKE_SOURCE_DIR}/extern)

# ---- Include paths ----
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${EXTERN_DIR}/glm
    ${EXTERN_DIR}/stb
    ${EXTERN_DIR}/noise
    ${EXTERN_DIR}
    ${EXTERN_DIR}/Vulkan-Headers/include
    ${EXTERN_DIR}/glfw/install/include
    ${EXTERN_DIR}/x11_headers
)

# ---- External libraries ----
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES IMPORTED_LOCATION ${EXTERN_DIR}/glfw/install/lib/libglfw3.a)

# Find X11 and other system libs
find_library(VULKAN_LIB vulkan PATHS /usr/lib/x86_64-linux-gnu ${EXTERN_DIR} NO_DEFAULT_PATH)
find_library(VULKAN_LIB vulkan)
find_library(DL_LIB dl)
find_library(PTHREAD_LIB pthread)
find_library(Z_LIB z)

# X11 libraries
set(X11_LIBS
    ${EXTERN_DIR}/libX11.so
    ${EXTERN_DIR}/libX11-xcb.so
    ${EXTERN_DIR}/libxcb.so
    ${EXTERN_DIR}/libXau.so
    ${EXTERN_DIR}/libXdmcp.so
    ${EXTERN_DIR}/libXrandr.so
    ${EXTERN_DIR}/libXinerama.so
    ${EXTERN_DIR}/libXcursor.so
    ${EXTERN_DIR}/libXi.so
    ${EXTERN_DIR}/libXext.so
    ${EXTERN_DIR}/libXfixes.so
    ${EXTERN_DIR}/libXrender.so
    ${EXTERN_DIR}/libXxf86vm.so
    ${EXTERN_DIR}/libxcb-shm.so
    ${EXTERN_DIR}/libxcb-util.so
    ${EXTERN_DIR}/libxcb-image.so
    ${EXTERN_DIR}/libxcb-keysyms.so
    ${EXTERN_DIR}/libxcb-randr.so
    ${EXTERN_DIR}/libxcb-sync.so
    ${EXTERN_DIR}/libxcb-xfixes.so
    ${EXTERN_DIR}/libxcb-xinerama.so
    ${EXTERN_DIR}/libxcb-shape.so
    ${EXTERN_DIR}/libxcb-xkb.so
    ${EXTERN_DIR}/libxcb-present.so
    ${EXTERN_DIR}/libxcb-dri3.so
    ${EXTERN_DIR}/libxcb-dri2.so
)

# ---- Sources ----
set(SOURCES
    src/main.cpp
    src/core/engine.cpp
    src/core/settings.cpp
    src/core/window.cpp
    src/core/input.cpp
    src/core/camera.cpp
    src/vulkan/vulkan_context.cpp
    src/vulkan/vulkan_renderer.cpp
    src/vulkan/vulkan_mesh.cpp
    src/vulkan/vulkan_pipeline.cpp
    src/world/block.cpp
    src/world/chunk.cpp
    src/world/chunk_mesh.cpp
    src/world/world.cpp
    src/world/terrain_gen.cpp
    src/world/tree_blueprint.cpp
    src/player/player.cpp
    src/inventory/inventory.cpp
    src/inventory/crafting.cpp
    src/core/texture_manager.cpp
    src/ui/ui_renderer.cpp
    src/ui/menu_state.cpp
    src/ui/pause_state.cpp
    src/ui/settings_state.cpp
)

set(HEADERS
    src/core/engine.hpp
    src/core/settings.hpp
    src/core/window.hpp
    src/core/input.hpp
    src/core/camera.hpp
    src/core/types.hpp
    src/core/vk_check.hpp
    src/vulkan/vulkan_context.hpp
    src/vulkan/vulkan_renderer.hpp
    src/vulkan/vulkan_mesh.hpp
    src/vulkan/vulkan_pipeline.hpp
    src/world/block.hpp
    src/world/chunk.hpp
    src/world/chunk_mesh.hpp
    src/world/world.hpp
    src/world/terrain_gen.hpp
    src/world/tree_blueprint.hpp
    src/player/player.hpp
    src/inventory/inventory.hpp
    src/inventory/crafting.hpp
    src/core/texture_manager.hpp
    src/ui/ui_renderer.hpp
    src/ui/menu_state.hpp
    src/ui/pause_state.hpp
    src/ui/settings_state.hpp
    src/ui/font_data.hpp
)

add_executable(moidcraft ${SOURCES} ${HEADERS})

target_link_libraries(moidcraft
    glfw
    ${VULKAN_LIB}
    ${DL_LIB}
    ${PTHREAD_LIB}
    ${Z_LIB}
    ${X11_LIBS}
    m
)

# Copy shaders and textures to build dir
file(COPY ${CMAKE_SOURCE_DIR}/shaders/ DESTINATION ${CMAKE_BINARY_DIR}/shaders/)
file(COPY ${CMAKE_SOURCE_DIR}/textures/ DESTINATION ${CMAKE_BINARY_DIR}/textures/)
