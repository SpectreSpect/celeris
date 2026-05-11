#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "vulkan_engine.h"
#include "vulkan_window.h"
#include "fps_camera_controller.h"

#include "point_cloud/point_cloud_pass.h"
#include "point_cloud/normal_cloud_pass.h"
#include "point_cloud/point_cloud_video.h"
#include "icp/gicp_pass.h"
#include "icp/voxel_point_map.h"
#include "icp/voxel_map_point_inserter.h"
#include "icp/voxel_point_map_reseter.h"
#include "point_cloud/generation/point_cloud_generator.h"
#include "gicp_test_clouds.h"
#include "icp/gicp.h"


int main() {
    VulkanEngine engine;
    VulkanWindow window(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    ui::init(&window, &engine);

    Camera camera = Camera();
    camera.position.z = 0;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 20;

    PointCloudPass point_cloud_pass;
    point_cloud_pass.create(engine);
    PointCloudGenerator point_cloud_generator;
    point_cloud_generator.create(engine);
    PointCloud generated_point_cloud;


    // int lidar_scan_width = 3600;
    int lidar_scan_width = 100;
    int lidar_scan_height = 16;

    generated_point_cloud.create(engine, lidar_scan_width * lidar_scan_height);

    uint32_t num_point_cloud_frames = 100;
    PointCloudFrame point_cloud_frames[num_point_cloud_frames];
    point_cloud_generator.generate_with_motion(point_cloud_frames, num_point_cloud_frames, lidar_scan_width, lidar_scan_height);

    GICPTestClouds gicp_test_clouds;
    gicp_test_clouds.create_roads(&engine);

    VoxelPointMap voxel_point_map;
    voxel_point_map.create(engine, 1500000, 1500000);
    VoxelPointMapReseter voxel_point_map_reseter;
    voxel_point_map_reseter.create(engine);
    voxel_point_map_reseter.reset(voxel_point_map);
    VoxelMapPointInserter voxel_map_point_inserter;
    voxel_map_point_inserter.create(engine);

    uint32_t test_frame = 0;
    // size_t last_frame_id = 80;
    size_t last_frame_id = 2;

    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);

    // voxel_map_point_inserter.insert(voxel_point_map, gicp_test_clouds.target_frame.point_cloud, gicp_test_clouds.target_frame.normal_buffer);

    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(-1, -1, -1);
    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(0, -0.1, 0);
    // point_cloud_frames[test_frame].point_cloud.position = glm::vec3(0, 0, 0);

    point_cloud_frames[0].point_cloud.color = glm::vec4(0, 0, 1, 1);
    point_cloud_frames[last_frame_id].point_cloud.color = glm::vec4(1, 0, 0, 1);

    // point_cloud_frames[80].point_cloud.rotation = glm::vec3(0.0f, 1.26295185, 0.0f);
    // point_cloud_frames[last_frame_id].point_cloud.rotation = glm::vec3(0.0f, 1, 0.0f);

    // point_cloud_frames[last_frame_id].get_normals(point_cloud_frames[last_frame_id].points, point_cloud_frames[last_frame_id].normals);
    // point_cloud_frames[last_frame_id].normal_buffer.update_data(point_cloud_frames[last_frame_id].normals.data(), point_cloud_frames[last_frame_id].normals.size() * sizeof(glm::vec4));



    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[0].point_cloud, point_cloud_frames[0].normal_buffer);
    voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[test_frame].point_cloud, point_cloud_frames[test_frame].normal_buffer);

    GICPPass gicp_pass;
    gicp_pass.create(engine);
    PointCloud voxel_map_point_cloud;
    voxel_map_point_cloud.create(engine, voxel_point_map.map_point_count);

    voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

    NormalCloudPass normal_cloud_pass;
    normal_cloud_pass.create(engine);
    normal_cloud_pass.normal_length = 0.5f;
    normal_cloud_pass.line_width_px = 2.0f;
    normal_cloud_pass.use_point_color = false;

    GICP gicp;

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
        // point_cloud_pass.render(gicp_test_clouds.source_frame.point_cloud, camera);
        // point_cloud_pass.render(point_cloud_frames[test_frame].point_cloud, camera);
        // point_cloud_pass.render(point_cloud_frames[last_frame_id].point_cloud, camera);

        // point_cloud_pass.render(point_cloud_frames[test_frame].point_cloud, camera);
        point_cloud_pass.render(point_cloud_frames[last_frame_id].point_cloud, camera);

        normal_cloud_pass.render(point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer, camera);
        // normal_cloud_pass.render(point_cloud_frames[test_frame].point_cloud, point_cloud_frames[test_frame].normal_buffer, camera);
        normal_cloud_pass.render(voxel_map_point_cloud, voxel_point_map.map_normal_buffer, camera);
        
        

        ImGui::Begin("Debug");

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // ImGui::InputFloat("Angle", &point_cloud_frames[last_frame_id].point_cloud.rotation.y);
        // ImGui::SliderFloat("Angle", &point_cloud_frames[80].point_cloud.rotation.y, 0.0f, 3.14f);

        if (ImGui::Button("Next frame")) {
            last_frame_id++;

            point_cloud_frames[last_frame_id].point_cloud.position = point_cloud_frames[last_frame_id - 1].point_cloud.position;
            point_cloud_frames[last_frame_id].point_cloud.rotation = point_cloud_frames[last_frame_id - 1].point_cloud.rotation;

            // point_cloud_frames[last_frame_id].point_cloud.position = glm::vec3(0, 0, 0);
            // point_cloud_frames[last_frame_id].point_cloud.rotation = glm::vec3(0, 0, 0);

            // gicp_pass.fit(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer, 10);
        }

        if (ImGui::Button("Insert frame")) {
            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("GICP step")) {
            // point_cloud_frames[last_frame_id].point_cloud.position = point_cloud_frames[last_frame_id - 1].point_cloud.position;
            // point_cloud_frames[last_frame_id].point_cloud.rotation = point_cloud_frames[last_frame_id - 1].point_cloud.rotation;

            // gicp_pass.step(voxel_point_map, gicp_test_clouds.source_frame.point_cloud, gicp_test_clouds.source_frame.normal_buffer);

            gicp_pass.step(voxel_point_map, point_cloud_frames[last_frame_id].point_cloud, point_cloud_frames[last_frame_id].normal_buffer);

            
            // gicp.step_test(point_cloud_frames[last_frame_id], point_cloud_frames[test_frame], 
            //                point_cloud_frames[last_frame_id].normals, point_cloud_frames[test_frame].normals);
            
            std::cout << point_cloud_frames[80].point_cloud.rotation.y << std::endl;

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
