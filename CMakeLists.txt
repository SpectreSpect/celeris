cmake_minimum_required(VERSION 3.20)

project(app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(app
  main.cpp
  camera.cpp
  mesh.cpp
  transformable.cpp
  fps_camera_controller.cpp
  imgui_layer.cpp
  vulkan_engine.cpp
  vulkan_window.cpp
  vulkan/shader_module.cpp
  vulkan/video_buffer.cpp
  vulkan/vulkan_vertex_layout.cpp
  vulkan/graphics_pipeline.cpp
  vulkan/pbr_renderer.cpp
  vulkan/image/texture2d.cpp
  vulkan/descriptor_set_layout.cpp
  vulkan/descriptor_pool.cpp
  vulkan/descriptor_set.cpp
  vulkan/descriptor_set_bundle.cpp
  vulkan/render_pass.cpp
  vulkan/render_target_2d.cpp
  vulkan/pbr/equirect_to_cubemap_pass.cpp
  vulkan/image/cubemap.cpp
  vulkan/command_pool.cpp
  vulkan/command_buffer.cpp
  vulkan/compute_pipeline.cpp
  vulkan/fence.cpp
  vulkan/resource_loader.cpp
  vulkan/image/image_resource.cpp
  vulkan/image/image_view.cpp
  vulkan/image/sampler.cpp
  skybox/skybox_pass.cpp
  vulkan/pbr/brdf_lut_generator.cpp
  vulkan/pbr/irradiance_map_generator.cpp
  vulkan/pbr/prefilter_map_generator.cpp
  vulkan/lighting_system/lighting_system.cpp
  point_cloud/point_cloud_pass.cpp
  point_cloud/point_cloud.cpp
  vulkan/compute_program.cpp
  point_cloud/point_cloud_video.cpp
  point_cloud/point_cloud_frame.cpp
  icp/gicp.cpp
  icp/gicp_pass.cpp
  icp/voxel_point_map.cpp
  icp/voxel_map_point_inserter.cpp
  icp/voxel_point_map_reseter.cpp
  icp/gicp_reductor.cpp
  point_cloud/generation/point_cloud_generator.cpp
  gicp_test_clouds.cpp
  point_cloud/normal_cloud_pass.cpp
)

target_link_libraries(app PRIVATE
  glfw
  glm::glm
  Vulkan::Vulkan
)

if(WIN32)
  target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE OFF)
endif()

# ---------------- ImGui ----------------
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
  FetchContent_Populate(imgui)
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

# ---------------- Shader compilation ----------------
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADERS_BIN_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADERS_RUNTIME_DIR "$<TARGET_FILE_DIR:app>/shaders")

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
  "${SHADERS_SRC_DIR}/*.vert"
  "${SHADERS_SRC_DIR}/*.frag"
  "${SHADERS_SRC_DIR}/*.comp"
)

set(SPIRV_OUTPUTS)

foreach(SHADER IN LISTS SHADER_SOURCES)
  file(RELATIVE_PATH REL_PATH "${SHADERS_SRC_DIR}" "${SHADER}")
  set(SPIRV_OUTPUT "${SHADERS_BIN_DIR}/${REL_PATH}.spv")

  get_filename_component(SPIRV_OUTPUT_DIR "${SPIRV_OUTPUT}" DIRECTORY)

  add_custom_command(
    OUTPUT "${SPIRV_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
    COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER}" -o "${SPIRV_OUTPUT}"
    DEPENDS "${SHADER}"
    COMMENT "Compiling shader ${REL_PATH}"
    VERBATIM
  )

  list(APPEND SPIRV_OUTPUTS "${SPIRV_OUTPUT}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
add_dependencies(app compile_shaders)

add_custom_target(copy_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_RUNTIME_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADERS_BIN_DIR}" "${SHADERS_RUNTIME_DIR}"
  DEPENDS compile_shaders
  COMMENT "Copying compiled shaders next to app"
)

add_dependencies(copy_shaders app)

# ---------------- Assets ----------------
set(ASSETS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_RUNTIME_DIR "$<TARGET_FILE_DIR:app>/assets")

add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ASSETS_SRC_DIR}"
          "${ASSETS_RUNTIME_DIR}"
  COMMENT "Copying assets next to app"
)

add_dependencies(copy_assets app)