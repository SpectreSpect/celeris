#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "vulkan_engine.h"
#include "vulkan_window.h"
#include "fps_camera_controller.h"

#include "point_cloud/point_cloud_pass.h"
#include "point_cloud/point_cloud_video.h"
#include "icp/gicp_pass.h"
#include "icp/voxel_point_map.h"
#include "icp/voxel_map_point_inserter.h"
#include "icp/voxel_point_map_reseter.h"
#include "point_cloud/generation/point_cloud_generator.h"
#include "gicp_test_clouds.h"


int main() {
    VulkanEngine engine;
    VulkanWindow window(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    ui::init(&window, &engine);

    Camera camera = Camera();
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 150;

    PointCloudPass point_cloud_pass;
    point_cloud_pass.create(engine);
    PointCloudGenerator point_cloud_generator;
    point_cloud_generator.create(engine);
    PointCloud generated_point_cloud;
    generated_point_cloud.create(engine, 3600 * 16);

    uint32_t num_point_cloud_frames = 100;
    PointCloudFrame point_cloud_frames[num_point_cloud_frames];
    point_cloud_generator.generate_with_motion(point_cloud_frames, num_point_cloud_frames);

    GICPTestClouds gicp_test_clouds;
    gicp_test_clouds.create_roads(&engine);

    VoxelPointMap voxel_point_map;
    voxel_point_map.create(engine, 1500000, 1500000);
    VoxelPointMapReseter voxel_point_map_reseter;
    voxel_point_map_reseter.create(engine);
    voxel_point_map_reseter.reset(voxel_point_map);
    VoxelMapPointInserter voxel_map_point_inserter;
    voxel_map_point_inserter.create(engine);
    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[0].point_cloud, point_cloud_frames[0].normal_buffer);

    voxel_map_point_inserter.insert(voxel_point_map, gicp_test_clouds.target_frame.point_cloud, gicp_test_clouds.target_frame.normal_buffer);

    GICPPass gicp_pass;
    gicp_pass.create(engine);
    PointCloud voxel_map_point_cloud;
    voxel_map_point_cloud.create(engine, voxel_point_map.map_point_count);

    voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

    size_t last_frame_id = 0;
    float timer = 0.0f;
    float last_frame = 0.0f;
    float start_time = (float)glfwGetTime();
    while (window.is_open()) {
        engine.poll_events();

        float currentFrame = (float)glfwGetTime() - start_time;
        float delta_time = currentFrame - last_frame;
        last_frame = currentFrame;

        camera_controller.update(&window, delta_time);

        engine.begin_frame(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
        if (!engine.frameInProgress) {
            continue;
        }

        ui::begin_frame();
        ui::update_mouse_mode(&window);
        
        // point_cloud_pass.render(point_cloud_frames[last_frame_id].point_cloud, camera);

        point_cloud_pass.render(voxel_map_point_cloud, camera);

        // point_cloud_pass.render(gicp_test_clouds.target_frame.point_cloud, camera);
        point_cloud_pass.render(gicp_test_clouds.source_frame.point_cloud, camera);

        ImGui::Begin("Debug");

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        if (ImGui::Button("Next frame")) {
            last_frame_id++;

            point_cloud_frames[last_frame_id].point_cloud.position = point_cloud_frames[last_frame_id - 1].point_cloud.position;
            point_cloud_frames[last_frame_id].point_cloud.rotation = point_cloud_frames[last_frame_id - 1].point_cloud.rotation;

            gicp_pass.fit(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer, 10);
            
            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);
            
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("GICP step")) {
            // point_cloud_frames[last_frame_id].point_cloud.position = point_cloud_frames[last_frame_id - 1].point_cloud.position;
            // point_cloud_frames[last_frame_id].point_cloud.rotation = point_cloud_frames[last_frame_id - 1].point_cloud.rotation;

            gicp_pass.step(voxel_point_map, gicp_test_clouds.source_frame.point_cloud, gicp_test_clouds.source_frame.normal_buffer);

            // gicp_pass.fit(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer, 10);
            
            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);
            
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }


        ImGui::End();

        ui::end_frame(engine.currentCommandBuffer);
        engine.end_frame();

        timer += delta_time;
    }

    vkDeviceWaitIdle(engine.device);
    ui::shutdown();
}
