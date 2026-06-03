cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)

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
  stb_image_impl.cpp
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
  vulkan_self/vulkan_render_pass.cpp
  vulkan_self/vulkan_framebuffer.cpp
  vulkan_self/vulkan_command_pool.cpp
  vulkan_self/vulkan_command_buffer.cpp
  vulkan_self/vulkan_fence.cpp
  vulkan_self/vulkan_semaphore.cpp
  vulkan_self/vulkan_shader_module.cpp
  vulkan_self/vulkan_buffer.cpp
  vulkan_self/vertex_layout_builder.cpp
  vulkan_self/vulkan_memory.cpp
  vulkan_self/vulkan_resource_loader.cpp
  vulkan_self/descriptor_set/descriptor_set_layout_builder.cpp
  vulkan_self/descriptor_set/descriptor_set_layout.cpp
  vulkan_self/descriptor_set/descriptor_pool_builder.cpp
  vulkan_self/descriptor_set/descriptor_pool.cpp
  vulkan_self/descriptor_set/descriptor_set.cpp
  vulkan_self/descriptor_set/descripter_set_writer.cpp
  vulkan_self/image/cpu_image.cpp
  vulkan_self/image/vulkan_image_view.cpp
  vulkan_self/image/vulkan_image.cpp
  vulkan_self/image/vulkan_sampler.cpp
  vulkan_self/image/vulkan_texture_2d.cpp
  vulkan_self/pipeline/vulkan_pipeline_layout.cpp
  vulkan_self/pipeline/pipeline.cpp
  vulkan_self/pipeline/pipeline_pass.cpp
  vulkan_self/pipeline/pipeline_pass_builder.cpp
  vulkan_self/pipeline/graphics_pipeline/graphics_pipeline_builder.cpp
  vulkan_self/pipeline/graphics_pipeline/graphics_pipeline.cpp
  vulkan_self/pipeline/compute_pipeline/compute_pipeline.cpp
  vulkan_self/material/material_system.cpp
  vulkan_self/material/blin_phong_material_layout.cpp
  vulkan_self/material/material_layout.cpp
  vulkan_self/material/material_pass.cpp
  vulkan_self/material/material_pass_builder.cpp
  vulkan_self/compute/compute_pass.cpp
  vulkan_self/compute/compute_pass_builder.cpp
  renderer/resources/frame_resources.cpp
  camera/camera.cpp
  camera/controllers/fps_camera_controller.cpp
  vulkan_self/vulkan_image_temp.cpp
  renderer/transform.cpp
  renderer/mesh.cpp
  renderer/render_object.cpp
  renderer/shader_manager.cpp
  renderer/material_manager.cpp
  renderer/compute_pass_manager.cpp
  renderer/pass_object.cpp
  renderer/pass_instance.cpp
  renderer/pass_writer.cpp
  renderer/instanced_render_object.cpp
  renderer/instance_batch.cpp
  renderer/instance_buffer_view.cpp
  renderer/texture_manager.cpp
  renderer/material_instance_manager.cpp
  renderer/renderer.cpp
  renderer/mesh_manager.cpp
  renderer/manager_bundle.cpp
  renderer/point_cloud/point_cloud.cpp
  renderer/scene.cpp
  renderer/scene_object.cpp
  renderer/point_cloud/lidar/lidar_scan.cpp
  renderer/point_cloud/lidar/lidar_video.cpp
  renderer/point_cloud/gicp/gicp_pass.cpp
  renderer/point_cloud/gicp/voxel_point_map.cpp
  renderer/point_cloud/gicp/voxel_map_point_inserter.cpp
  renderer/point_cloud/gicp/voxel_map_point_reseter.cpp
  renderer/point_cloud/gicp/gicp_reductor.cpp
  imgui_layer.cpp
  renderer/lighting_system/lighting_system.cpp
  voxel_grid_vulkan/voxel_grid.cpp
  vulkan_self/buffer_filler.cpp
)

target_compile_features(app PRIVATE cxx_std_20)

# ---------------- ImGui (FetchContent, no submodule) ----------------

include(FetchContent)

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
  ${imgui_SOURCE_DIR}/imgui_demo.cpp

  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(app PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

# ---------------- Vulkan shaders ----------------

find_program(GLSLC_EXECUTABLE
  NAMES
    glslc
    glslc.exe
  HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VULKAN_SDK}/bin"
)

find_program(GLSLANG_VALIDATOR_EXECUTABLE
  NAMES
    glslangValidator
    glslangValidator.exe
  HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VULKAN_SDK}/bin"
)

if(GLSLC_EXECUTABLE)
  set(VULKAN_SHADER_COMPILER "${GLSLC_EXECUTABLE}")
  set(VULKAN_SHADER_COMPILER_KIND "glslc")
elseif(GLSLANG_VALIDATOR_EXECUTABLE)
  set(VULKAN_SHADER_COMPILER "${GLSLANG_VALIDATOR_EXECUTABLE}")
  set(VULKAN_SHADER_COMPILER_KIND "glslangValidator")
else()
  message(FATAL_ERROR
    "Could not find Vulkan shader compiler. "
    "Install Vulkan SDK and make sure glslc/glslangValidator is available. "
    "On Windows, check that VULKAN_SDK points to your Vulkan SDK directory."
  )
endif()

message(STATUS "Vulkan shader compiler: ${VULKAN_SHADER_COMPILER}")
message(STATUS "Vulkan shader compiler kind: ${VULKAN_SHADER_COMPILER_KIND}")

set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADERS_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
  "${SHADERS_SRC_DIR}/*.vert"
  "${SHADERS_SRC_DIR}/*.frag"
  "${SHADERS_SRC_DIR}/*.comp"
)

file(GLOB_RECURSE SHADER_INCLUDE_FILES CONFIGURE_DEPENDS
  "${SHADERS_SRC_DIR}/*.inc"
)

set(SPIRV_OUTPUTS)

foreach(SHADER ${SHADER_SOURCES})
  file(RELATIVE_PATH SHADER_REL_PATH "${SHADERS_SRC_DIR}" "${SHADER}")

  set(SPIRV_OUTPUT "${SHADERS_BUILD_DIR}/${SHADER_REL_PATH}.spv")

  get_filename_component(SPIRV_OUTPUT_DIR "${SPIRV_OUTPUT}" DIRECTORY)

  if(VULKAN_SHADER_COMPILER_KIND STREQUAL "glslc")
    add_custom_command(
      OUTPUT "${SPIRV_OUTPUT}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
      COMMAND "${VULKAN_SHADER_COMPILER}"
              "-I${SHADERS_SRC_DIR}"
              "${SHADER}"
              -o "${SPIRV_OUTPUT}"
      DEPENDS "${SHADER}" ${SHADER_INCLUDE_FILES}
      COMMENT "Compiling shader ${SHADER_REL_PATH}"
      VERBATIM
    )
  else()
    add_custom_command(
      OUTPUT "${SPIRV_OUTPUT}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
      COMMAND "${VULKAN_SHADER_COMPILER}"
              -V
              "-I${SHADERS_SRC_DIR}"
              "${SHADER}"
              -o "${SPIRV_OUTPUT}"
      DEPENDS "${SHADER}" ${SHADER_INCLUDE_FILES}
      COMMENT "Compiling shader ${SHADER_REL_PATH}"
      VERBATIM
    )
  endif()

  list(APPEND SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
endforeach()

add_custom_target(compile_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_BUILD_DIR}"
  DEPENDS ${SPIRV_OUTPUTS}
  COMMENT "Compiling Vulkan shaders"
)

add_dependencies(app compile_shaders)

add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:app>/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SHADERS_BUILD_DIR}"
          "$<TARGET_FILE_DIR:app>/shaders"
  COMMENT "Copying compiled shaders next to app"
)

# ---------------- Assets ----------------

set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")

if(EXISTS "${ASSETS_SRC}")
  add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:app>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SRC}"
            "$<TARGET_FILE_DIR:app>/assets"
    COMMENT "Copying assets next to app"
  )
endif()

# ---------------- Link libraries ----------------

target_link_libraries(app PRIVATE
  glfw
  GLEW::GLEW
  glm::glm
  OpenGL::GL
  Vulkan::Vulkan
)

if(WIN32)
  target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE OFF)
endif()