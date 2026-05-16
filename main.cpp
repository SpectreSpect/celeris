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

    std::string camera_transformations_path = "/home/spectre/TEMP_lidar_output_mesh/camera_transformations/camera_transformations.txt";

    PointCloudVideo point_cloud_video = PointCloudVideo();
    point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 0, -1);

    // for (int i = 60; i <= 81; i++) {
    //     point_cloud_video.frames[i].point_cloud.position = glm::vec3(0, 0, 0);
    //     point_cloud_video.frames[i].point_cloud.rotation = glm::vec3(0, 0, 0);
    // }

    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 200);
    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 70, 150);
    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 100, 200);

    // int lidar_scan_width = 3600;
    int lidar_scan_width = 100;
    // int lidar_scan_height = 16;
    int lidar_scan_height = 16;

    generated_point_cloud.create(engine, lidar_scan_width * lidar_scan_height);

    uint32_t num_point_cloud_frames = 100;
    PointCloudFrame point_cloud_frames[num_point_cloud_frames];
    // point_cloud_generator.generate_with_motion(point_cloud_frames, num_point_cloud_frames, lidar_scan_width, lidar_scan_height);
    point_cloud_generator.generate_from_camera_transform_file(point_cloud_frames, num_point_cloud_frames, 
        lidar_scan_width, lidar_scan_height, camera_transformations_path);

    GICPTestClouds gicp_test_clouds;
    // gicp_test_clouds.create_roads(&engine);
    gicp_test_clouds.create_points(&engine);

    VoxelPointMap voxel_point_map;
    voxel_point_map.create(engine, 1500000, 1500000);
    VoxelPointMapReseter voxel_point_map_reseter;
    voxel_point_map_reseter.create(engine);
    voxel_point_map_reseter.reset(voxel_point_map);
    VoxelMapPointInserter voxel_map_point_inserter;
    voxel_map_point_inserter.create(engine);

    uint32_t test_frame = 0;
    size_t current_frame_id = 0;
    size_t last_inserted_frame_id = current_frame_id;

    auto& frames = point_cloud_video.frames;

    // Important: save original first frame pose before overwriting it.
    glm::vec3 first_position = frames[0].point_cloud.position;
    glm::quat first_rotation = glm::normalize(frames[0].point_cloud.rotation);

    for (int i = static_cast<int>(frames.size()) - 1; i >= 1; --i) {
        glm::vec3 p_prev = frames[i - 1].point_cloud.position;
        glm::vec3 p_curr = frames[i].point_cloud.position;

        glm::quat q_prev = glm::normalize(frames[i - 1].point_cloud.rotation);
        glm::quat q_curr = glm::normalize(frames[i].point_cloud.rotation);

        // Optional: avoid quaternion sign discontinuity.
        // q and -q represent the same rotation.
        if (glm::dot(q_prev, q_curr) < 0.0f) {
            q_curr = -q_curr;
        }

        glm::vec3 delta_position = p_curr - p_prev;

        // Since your update convention is:
        // q_new = dq * q_old
        glm::quat delta_rotation = glm::normalize(q_curr * glm::inverse(q_prev));

        frames[i].point_cloud.position = delta_position;
        frames[i].point_cloud.rotation = delta_rotation;
    }

    frames[0].point_cloud.position = glm::vec3(0.0f);
    frames[0].point_cloud.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);


    // point_cloud_video.frames[last_inserted_frame_id].point_cloud.position = glm::vec3(0, 0, 0);
    // point_cloud_video.frames[last_inserted_frame_id].point_cloud.rotation = glm::vec3(0, 0, 0);


    // size_t current_frame_id = 1;

    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(0.0f, 1, 0.0f);

    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[last_inserted_frame_id].point_cloud, point_cloud_video.frames[last_inserted_frame_id].normal_buffer);


    // voxel_map_point_inserter.insert(voxel_point_map, gicp_test_clouds.target_frame.point_cloud, gicp_test_clouds.target_frame.normal_buffer);

    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(-1, -1, -1);
    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(0, -0.1, 0);
    // point_cloud_frames[test_frame].point_cloud.position = glm::vec3(0, 0, 0);

    point_cloud_frames[0].point_cloud.color = glm::vec4(0, 0, 1, 1);
    point_cloud_frames[current_frame_id].point_cloud.color = glm::vec4(1, 0, 0, 1);

    std::ofstream out;

    // point_cloud_frames[80].point_cloud.rotation = glm::vec3(0.0f, 1.26295185, 0.0f);
    // point_cloud_frames[current_frame_id].point_cloud.rotation = glm::vec3(0.0f, 1.2, 0.0f);

    // point_cloud_frames[current_frame_id].get_normals(point_cloud_frames[current_frame_id].points, point_cloud_frames[current_frame_id].normals);
    // point_cloud_frames[current_frame_id].normal_buffer.update_data(point_cloud_frames[current_frame_id].normals.data(), point_cloud_frames[current_frame_id].normals.size() * sizeof(glm::vec4));

    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[0].point_cloud, point_cloud_frames[0].normal_buffer);
    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[test_frame].point_cloud, point_cloud_frames[test_frame].normal_buffer);

    GICPPass gicp_pass;
    gicp_pass.create(engine);
    PointCloud voxel_map_point_cloud;
    voxel_map_point_cloud.create(engine, voxel_point_map.map_point_count);

    voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

    PointCloudFrame voxel_map_frame;
    voxel_point_map.get_point_cloud_frame(&voxel_map_frame);
    
    int recording_fps = 5;

    NormalCloudPass normal_cloud_pass;
    normal_cloud_pass.create(engine);
    normal_cloud_pass.normal_length = 0.5f;
    normal_cloud_pass.line_width_px = 2.0f;
    normal_cloud_pass.use_point_color = false;

    uint32_t current_recording_frame = 0;
    bool is_recording = false;

    GICP gicp;

    float timer = 0.0f;
    float last_frame = 0.0f;
    float start_time = (float)glfwGetTime();
    long long global_frame_id = 0;
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
        
        // point_cloud_pass.render(point_cloud_frames[current_frame_id].point_cloud, camera);

        point_cloud_pass.render(voxel_map_point_cloud, camera);

        // point_cloud_pass.render(gicp_test_clouds.target_frame.point_cloud, camera);
        // point_cloud_pass.render(voxel_map_frame.point_cloud, camera);
    
        // point_cloud_pass.render(gicp_test_clouds.source_frame.point_cloud, camera);


        // point_cloud_pass.render(point_cloud_frames[current_frame_id].point_cloud, camera);
        point_cloud_pass.render(point_cloud_video.frames[current_frame_id].point_cloud, camera);

        // point_cloud_pass.render(point_cloud_frames[test_frame].point_cloud, camera);
        // point_cloud_pass.render(point_cloud_frames[current_frame_id].point_cloud, camera);

        // normal_cloud_pass.render(point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer, camera);
        // normal_cloud_pass.render(point_cloud_frames[test_frame].point_cloud, point_cloud_frames[test_frame].normal_buffer, camera);
        // normal_cloud_pass.render(voxel_map_point_cloud, voxel_point_map.map_normal_buffer, camera);

        ImGui::Begin("Debug");

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Current frame: %ld", current_frame_id);
        ImGui::Text("Last inserted frame: %ld", last_inserted_frame_id);
        // ImGui::Text("Current rotation: (%.5f, %.5f, %.5f)", point_cloud_video.frames[current_frame_id].point_cloud.rotation.x, point_cloud_video.frames[current_frame_id].point_cloud.rotation.y, point_cloud_video.frames[current_frame_id].point_cloud.rotation.z);

        // ImGui::InputFloat("Angle", &point_cloud_frames[current_frame_id].point_cloud.rotation.y);
        // ImGui::SliderFloat("Angle", &point_cloud_frames[80].point_cloud.rotation.y, 0.0f, 3.14f);

        if (ImGui::Button("Start recording")) {
            is_recording = true;
        }
        if (ImGui::Button("End recording")) {
            is_recording = false;
        }

        if (is_recording) {
            if (global_frame_id % recording_fps == 0) {

                std::cout << "frame" << std::endl;

                if (current_recording_frame <= 0) {
                    out.open("/home/spectre/TEMP_lidar_output_mesh/camera_transformations/camera_transformations.txt");
                }

                if (!out) {
                    throw std::runtime_error("Failed to open camera transformation file");
                }

                if (current_recording_frame <= 0) {
                    out << std::fixed << std::setprecision(9);
                    PointCloudGenerator::write_camera_transform_header(out);
                }
                

                // Every frame:
                PointCloudGenerator::write_camera_transform(out, current_recording_frame, camera);

                current_recording_frame++;
            }
        }

        if (ImGui::Button("Next frame")) {
            current_frame_id++;

            // point_cloud_video.frames[current_frame_id].point_cloud.color = glm::vec4(1, 0, 0, 1);

            // point_cloud_frames[current_frame_id].point_cloud.position = point_cloud_frames[current_frame_id - 1].point_cloud.position;
            // point_cloud_frames[current_frame_id].point_cloud.rotation = point_cloud_frames[current_frame_id - 1].point_cloud.rotation;

            if (current_frame_id > 0) {
                auto& curr = point_cloud_video.frames[current_frame_id].point_cloud;
                const auto& prev = point_cloud_video.frames[current_frame_id - 1].point_cloud;

                curr.position = prev.position + curr.position;

                curr.rotation = glm::normalize(curr.rotation * prev.rotation);
            }


            // gicp_pass.fit(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer, 10);

            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);


            // if (current_frame_id > last_inserted_frame_id) {
            //     bool x_rot_ok = std::abs(point_cloud_video.frames[current_frame_id].point_cloud.rotation.x) < 0.1;
            //     bool z_rot_ok = std::abs(point_cloud_video.frames[current_frame_id].point_cloud.rotation.z) < 0.1;

            //     if (!x_rot_ok || !z_rot_ok)
            //         std::cout << "SOS" << std::endl;

            //     if (x_rot_ok && z_rot_ok) {
            //         gicp_pass.fit(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer, 20);

            //         bool x_rot_ok2 = std::abs(point_cloud_video.frames[current_frame_id].point_cloud.rotation.x) < 0.1;
            //         bool z_rot_ok2 = std::abs(point_cloud_video.frames[current_frame_id].point_cloud.rotation.z) < 0.1;

            //         if (!x_rot_ok2 || !z_rot_ok2)
            //             std::cout << "SOS2" << std::endl;

            //         if (x_rot_ok2 && z_rot_ok2) {
            //             voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);
            //             last_inserted_frame_id = current_frame_id;
            //             voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
            //         }    
            //     }
            // }


            // gicp_pass.fit(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer, 20);
            gicp_pass.fit(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer, 10);

            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);
            last_inserted_frame_id = current_frame_id;
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("Previous frame")) {
            current_frame_id--;
        }

        if (ImGui::Button("Insert frame")) {
            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            // last_inserted_frame_id = current_frame_id;
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

            // voxel_point_map.get_point_cloud_frame(&voxel_map_frame);

            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);
            last_inserted_frame_id = current_frame_id;
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("GICP step")) {
            // point_cloud_frames[current_frame_id].point_cloud.position = point_cloud_frames[current_frame_id - 1].point_cloud.position;
            // point_cloud_frames[current_frame_id].point_cloud.rotation = point_cloud_frames[current_frame_id - 1].point_cloud.rotation;

            // gicp_pass.step(voxel_point_map, gicp_test_clouds.source_frame.point_cloud, gicp_test_clouds.source_frame.normal_buffer);

            // gicp_pass.step(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            gicp_pass.step(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);

            
            // gicp.step_test(point_cloud_frames[current_frame_id], point_cloud_frames[test_frame], 
            //                point_cloud_frames[current_frame_id].normals, point_cloud_frames[test_frame].normals);

            // gicp.step_test(point_cloud_frames[current_frame_id], voxel_map_frame, 
            //         point_cloud_frames[current_frame_id].normals, voxel_map_frame.normals);
            
            // gicp.step_test(gicp_test_clouds.source_frame, gicp_test_clouds.target_frame, 
            //                gicp_test_clouds.source_frame.normals, gicp_test_clouds.target_frame.normals);
            
            // gicp_test_clouds.target_frame.point_cloud
            
            // std::cout << point_cloud_frames[80].point_cloud.rotation.y << std::endl;

            // gicp_pass.fit(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer, 10);
            
            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }


        ImGui::End();

        ui::end_frame(engine.currentCommandBuffer);
        engine.end_frame();

        timer += delta_time;
        global_frame_id++;
    }

    vkDeviceWaitIdle(engine.device);
    ui::shutdown();
}

