cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: silence OpenGL CMP0072 warning on Ubuntu
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(app
  main.cpp
  vulkan_self/vulkan_engine.cpp
  vulkan_self/colored_string/colored_string.cpp
  vulkan_self/colored_string/multicolor_string.cpp
  vulkan_self/colored_string/colored_string_stream.cpp
  vulkan_self/logger/logger.cpp
  vulkan_self/logger/level_marker.cpp
  vulkan_self/glfw_context.cpp
  vulkan_self/window.cpp
  vulkan_self/vulkan_instance.cpp
  vulkan_self/vulkan_surface.cpp
  vulkan_self/vulkan_physical_device.cpp
  vulkan_self/vulkan_device.cpp
  vulkan_self/vulkan_queue.cpp
  vulkan_self/vulkan_swapchain.cpp
  vulkan_self/vulkan_image_view.cpp
  vulkan_self/vulkan_render_pass.cpp
  vulkan_self/vulkan_framebuffer.cpp
  vulkan_self/vulkan_command_pool.cpp
  vulkan_self/vulkan_command_buffer.cpp
  vulkan_self/vulkan_fence.cpp
  vulkan_self/vulkan_semaphore.cpp
  vulkan_self/vulkan_shader_module.cpp
  vulkan_self/vulkan_pipeline_layout.cpp
  vulkan_self/vulkan_pipeline.cpp
  vulkan_self/vulkan_buffer.cpp
  vulkan_self/vertex_layout_builder.cpp
  vulkan_self/vulkan_memory.cpp
  vulkan_self/vulkan_resource_loader.cpp
)

target_compile_features(app PRIVATE cxx_std_20)

# ---------------- ImGui (FetchContent, no submodule) ----------------
include(FetchContent)

# Kept at your old pin to minimize code churn.
# You can update later after the Vulkan integration is stable.
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.9
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_MakeAvailable(imgui)
endif()

target_sources(app PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp        # optional, but useful while integrating

  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(app PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

find_program(GLSLANG_VALIDATOR
  NAMES glslangValidator glslangValidator.exe
  HINTS
    ENV VULKAN_SDK
  PATH_SUFFIXES
    Bin
    bin
  REQUIRED
)

set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADERS_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
  "${SHADERS_SRC_DIR}/*.vert"
  "${SHADERS_SRC_DIR}/*.frag"
  "${SHADERS_SRC_DIR}/*.comp"
)

set(SPIRV_OUTPUTS)

foreach(SHADER ${SHADER_SOURCES})
  get_filename_component(SHADER_NAME "${SHADER}" NAME)
  set(SPIRV_OUTPUT "${SHADERS_BUILD_DIR}/${SHADER_NAME}.spv")

  add_custom_command(
    OUTPUT "${SPIRV_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_BUILD_DIR}"
    COMMAND "${GLSLANG_VALIDATOR}" -V "${SHADER}" -o "${SPIRV_OUTPUT}"
    DEPENDS "${SHADER}"
    COMMENT "Compiling shader ${SHADER_NAME}"
    VERBATIM
  )

  list(APPEND SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(app compile_shaders)

# Копируем уже скомпилированные .spv рядом с app.exe / app
add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:app>/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SHADERS_BUILD_DIR}"
          "$<TARGET_FILE_DIR:app>/shaders"
  COMMENT "Copying compiled shaders next to app"
)


set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")
add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ASSETS_SRC}"
          "$<TARGET_FILE_DIR:app>/assets"
  COMMENT "Copying assets next to app"
)
add_dependencies(copy_assets app)

# set(SHADERS_SRC "${CMAKE_SOURCE_DIR}/shaders")
# add_custom_target(copy_shaders ALL
#   COMMAND ${CMAKE_COMMAND} -E copy_directory
#           "${SHADERS_SRC}"
#           "$<TARGET_FILE_DIR:app>/shaders"
#   COMMENT "Copying shaders next to app"
# )
# add_dependencies(copy_shaders app)

target_link_libraries(app PRIVATE
  glfw
  GLEW::GLEW
  glm::glm
  OpenGL::GL
  Vulkan::Vulkan
)

if (WIN32)
  target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE OFF)
endif()
