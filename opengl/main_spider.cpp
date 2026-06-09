#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"
#include "opengl_engine.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"
#include "sphere.h"
#include "cube.h"
#include "quad.h"
#include "texture_manager.h"
#include "pbr_skybox.h"
#include <algorithm>
#include <random>
#include "third_person_camera_controller.h"
#include "spider.h"



float clear_col[4] = {0, 0, 0, 1};

int main() {
    OpenGLEngine engine = OpenGLEngine();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);

    // Camera camera;
    // ThirdPersonController camera_controller(&camera);

    // camera_controller.target_position = glm::vec3(0.0f, 0.0f, 0.0f);
    // camera_controller.distance = 15.0f;
    // camera_controller.look_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    
    camera_controller.speed = 50;

    PBRSkybox skybox = PBRSkybox(engine.texture_manager->st_peters_square_night_4k);

    CircleCloud cicle_cloud = CircleCloud();

    std::vector<CirlceInstance> cicle_instances;

    float platform_size = 250;
    for (int x = 0; x < platform_size; x++)
        for (int z = 0; z < platform_size; z++) {
            CirlceInstance cicle_instance;
            cicle_instance.color = glm::vec4(0.9f, 0.9f, 0.9f, 1);
            cicle_instance.normal = glm::vec4(0, 1, 0, 1);
            cicle_instance.position_radius = glm::vec4(x - platform_size / 2.0f, 0, z - platform_size / 2.0f, 0.5f);

            cicle_instances.push_back(cicle_instance);
        }
    
    cicle_cloud.set_instances(cicle_instances);

    Spider spider = Spider(4, 4, 2.0f);

    float last_frame = 0.0f;
    float timer = 0.0f;
    int cur_frame = 100;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        window.draw(&skybox, &camera);

        skybox.attach_environment(*engine.default_program);
        skybox.attach_environment(*engine.default_circle_program);

        window.draw(&cicle_cloud, &camera);

        spider.update(delta_time);
        window.draw(&spider, &camera);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        if (ImGui::Button("Stop spider movement"))
        {
            spider.velocity = glm::vec3(0, 0, 0);
        }

        if (ImGui::Button("Start spider movement"))
        {
            spider.velocity = glm::vec3(10, 0, 10);
        }


        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
