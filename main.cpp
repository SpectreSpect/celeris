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
    point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 60, 80);

    // for (int i = 60; i <= 81; i++) {
    //     point_cloud_video.frames[i].point_cloud.position = glm::vec3(0, 0, 0);
    //     point_cloud_video.frames[i].point_cloud.rotation = glm::vec3(0, 0, 0);
    // }


    // point_cloud_video.frames[0].point_cloud.position  = glm::vec3( 0.000000f,  0.000000f,  0.000000f);
    // point_cloud_video.frames[0].point_cloud.rotation  = glm::vec3( 0.000000f, -0.000000f,  0.000000f);

    // point_cloud_video.frames[1].point_cloud.position  = glm::vec3( 0.095028f,  0.009074f,  0.000337f);
    // point_cloud_video.frames[1].point_cloud.rotation  = glm::vec3( 0.000915f,  0.001505f, -0.001687f);

    // point_cloud_video.frames[2].point_cloud.position  = glm::vec3( 0.070650f,  0.005823f, -0.033629f);
    // point_cloud_video.frames[2].point_cloud.rotation  = glm::vec3( 0.000805f,  0.001532f, -0.001894f);

    // point_cloud_video.frames[3].point_cloud.position  = glm::vec3( 0.025666f,  0.002121f, -0.077654f);
    // point_cloud_video.frames[3].point_cloud.rotation  = glm::vec3( 0.000695f, -0.001566f, -0.002057f);

    // point_cloud_video.frames[4].point_cloud.position  = glm::vec3( 0.029731f,  0.005125f, -0.116425f);
    // point_cloud_video.frames[4].point_cloud.rotation  = glm::vec3( 0.000797f,  0.006839f, -0.001915f);

    // point_cloud_video.frames[5].point_cloud.position  = glm::vec3( 0.139136f,  0.021429f, -0.063864f);
    // point_cloud_video.frames[5].point_cloud.rotation  = glm::vec3( 0.001856f,  0.005669f, -0.001930f);

    // point_cloud_video.frames[6].point_cloud.position  = glm::vec3(-1.388532f, -0.033575f,  0.014769f);
    // point_cloud_video.frames[6].point_cloud.rotation  = glm::vec3(-0.002613f, -0.036597f, -0.007652f);

    // point_cloud_video.frames[7].point_cloud.position  = glm::vec3(-1.385556f, -0.033877f,  0.006842f);
    // point_cloud_video.frames[7].point_cloud.rotation  = glm::vec3(-0.002556f, -0.038374f, -0.007655f);

    // point_cloud_video.frames[8].point_cloud.position  = glm::vec3(-1.379630f, -0.032513f, -0.002641f);
    // point_cloud_video.frames[8].point_cloud.rotation  = glm::vec3(-0.002443f, -0.041210f, -0.007660f);

    // point_cloud_video.frames[9].point_cloud.position  = glm::vec3(-1.375696f, -0.030153f, -0.018171f);
    // point_cloud_video.frames[9].point_cloud.rotation  = glm::vec3(-0.002258f, -0.043314f, -0.007649f);

    // point_cloud_video.frames[10].point_cloud.position = glm::vec3(-1.404806f, -0.029886f, -0.074445f);
    // point_cloud_video.frames[10].point_cloud.rotation = glm::vec3(-0.002125f, -0.040264f, -0.007725f);

    // point_cloud_video.frames[11].point_cloud.position = glm::vec3(-1.675376f, -0.074342f, -0.189398f);
    // point_cloud_video.frames[11].point_cloud.rotation = glm::vec3(-0.004261f, -0.039760f, -0.009183f);

    // point_cloud_video.frames[12].point_cloud.position = glm::vec3( 4.313474f,  0.267748f, -2.111527f);
    // point_cloud_video.frames[12].point_cloud.rotation = glm::vec3( 0.017267f, -0.097807f,  0.017469f);

    // point_cloud_video.frames[13].point_cloud.position = glm::vec3( 4.158283f,  0.225535f, -2.184684f);
    // point_cloud_video.frames[13].point_cloud.rotation = glm::vec3( 0.015443f, -0.098572f,  0.015070f);

    // point_cloud_video.frames[14].point_cloud.position = glm::vec3( 6.327390f,  0.379787f, -6.171457f);
    // point_cloud_video.frames[14].point_cloud.rotation = glm::vec3( 0.039429f,  0.330813f,  0.029762f);

    // point_cloud_video.frames[15].point_cloud.position = glm::vec3( 6.416764f,  0.318105f, -6.217488f);
    // point_cloud_video.frames[15].point_cloud.rotation = glm::vec3(-0.082768f,  0.514324f,  0.006963f);

    // point_cloud_video.frames[16].point_cloud.position = glm::vec3( 3.460682f,  0.067491f, -5.862047f);
    // point_cloud_video.frames[16].point_cloud.rotation = glm::vec3(-0.091348f,  0.547841f, -0.014060f);

    // point_cloud_video.frames[17].point_cloud.position = glm::vec3( 3.493831f,  0.089604f, -5.941942f);
    // point_cloud_video.frames[17].point_cloud.rotation = glm::vec3(-0.088392f,  0.532582f, -0.010648f);

    // point_cloud_video.frames[18].point_cloud.position = glm::vec3( 3.482988f,  0.090200f, -5.962421f);
    // point_cloud_video.frames[18].point_cloud.rotation = glm::vec3(-0.087656f,  0.520581f, -0.009437f);

    // point_cloud_video.frames[19].point_cloud.position = glm::vec3( 4.080833f,  0.044658f, -7.889926f);
    // point_cloud_video.frames[19].point_cloud.rotation = glm::vec3(-0.079607f,  0.794677f, -0.014218f);

    // point_cloud_video.frames[20].point_cloud.position = glm::vec3( 2.179936f, -0.317707f, -12.647614f);
    // point_cloud_video.frames[20].point_cloud.rotation = glm::vec3(-0.065356f,  1.140075f, -0.057718f);



    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 200);
    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 70, 150);
    // point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 100, 200);

    int lidar_scan_width = 3600;
    // int lidar_scan_width = 100;
    int lidar_scan_height = 16;

    generated_point_cloud.create(engine, lidar_scan_width * lidar_scan_height);

    uint32_t num_point_cloud_frames = 200;
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


    // point_cloud_video.frames[last_inserted_frame_id].point_cloud.position = glm::vec3(0, 0, 0);
    // point_cloud_video.frames[last_inserted_frame_id].point_cloud.rotation = glm::vec3(0, 0, 0);


    // size_t current_frame_id = 1;

    // point_cloud_frames[test_frame].point_cloud.rotation = glm::vec3(0.0f, 1, 0.0f);

    // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
    voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[last_inserted_frame_id].point_cloud, point_cloud_video.frames[last_inserted_frame_id].normal_buffer);




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
            

            // point_cloud_video.frames[current_frame_id].point_cloud.position = point_cloud_video.frames[current_frame_id - 1].point_cloud.position;
            // point_cloud_video.frames[current_frame_id].point_cloud.rotation = point_cloud_video.frames[current_frame_id - 1].point_cloud.rotation;


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




            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);
            // last_inserted_frame_id = current_frame_id;
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("Previous frame")) {
            current_frame_id--;
        }

        if (ImGui::Button("Insert frame")) {
            voxel_map_point_inserter.insert(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            last_inserted_frame_id = current_frame_id;
            voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

            voxel_point_map.get_point_cloud_frame(&voxel_map_frame);

            // voxel_map_point_inserter.insert(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);
            // last_inserted_frame_id = current_frame_id;
            // voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);
        }

        if (ImGui::Button("GICP step")) {
            // point_cloud_frames[current_frame_id].point_cloud.position = point_cloud_frames[current_frame_id - 1].point_cloud.position;
            // point_cloud_frames[current_frame_id].point_cloud.rotation = point_cloud_frames[current_frame_id - 1].point_cloud.rotation;

            // gicp_pass.step(voxel_point_map, gicp_test_clouds.source_frame.point_cloud, gicp_test_clouds.source_frame.normal_buffer);

            gicp_pass.step(voxel_point_map, point_cloud_frames[current_frame_id].point_cloud, point_cloud_frames[current_frame_id].normal_buffer);
            // gicp_pass.step(voxel_point_map, point_cloud_video.frames[current_frame_id].point_cloud, point_cloud_video.frames[current_frame_id].normal_buffer);

            
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

