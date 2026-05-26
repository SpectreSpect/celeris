#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/vulkan_buffer.h"
#include "vulkan_self/vulkan_resource_loader.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/descriptor_set/descriptor_set.h"
#include "vulkan_self/pipeline/vulkan_pipeline_layout.h"
#include "vulkan_self/pipeline/graphics_pipeline/graphics_pipeline.h"
#include "vulkan_self/pipeline/compute_pipeline/compute_pipeline.h"
#include "vulkan_self/pipeline/pipeline_pass_builder.h"
#include "vulkan_self/compute/compute_pass.h"
#include "vulkan_self/image/vulkan_image.h"
#include "vulkan_self/image/cpu_image.h"
#include "vulkan_self/image/vulkan_texture_2d.h"
#include "path_utils.h"
#include "vulkan_self/material/material_system.h"
#include "vulkan_self/material/material_instance.h"
#include "renderer/resources/frame_resources.h"
#include "camera/camera.h"
#include "camera/controllers/fps_camera_controller.h"
#include "renderer/transform.h"
#include "renderer/mesh.h"
#include "renderer/render_object.h"
#include "renderer/transform_push_constants.h"
#include "vulkan_self/material/blin_phong_material_pass.h"
#include "vulkan_self/material/material_instance_temp.h"
#include "vulkan_self/material/blinn_phong_material_instance.h"
#include "vulkan_self/material/unlit_material_instance.h"
#include "vulkan_self/material/material_instance.h"
#include "renderer/shader_manager.h"
#include "renderer/material_manager.h"
#include "vulkan_self/compute/compute_pass_builder.h"
#include "renderer/compute_pass_manager.h"
#include "renderer/compute_pass_instance.h"
#include "vulkan_self/material/material_buffer.h"
#include "renderer/instanced_render_object.h"
#include "renderer/instance_batch.h"
#include "renderer/texture_manager.h"
#include "renderer/material_instance_manager.h"
#include "renderer/point_cloud/point_instance.h"
#include "renderer/renderer.h"
#include "renderer/mesh_manager.h"
#include "renderer/manager_bundle.h"
#include "renderer/point_cloud/point_cloud.h"
#include "renderer/scene.h"
#include "renderer/point_cloud/lidar/lidar_scan.h"
#include "renderer/point_cloud/lidar/lidar_video.h"
#include "renderer/point_cloud/gicp/gicp_pass.h"
#include "renderer/point_cloud/gicp/voxel_point_map.h"
#include "renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "renderer/point_cloud/gicp/voxel_map_point_reseter.h"

#include <vector>


float srgb_to_linear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }

    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

VkClearValue make_clear_color_srgb(glm::vec4 color) {
    VkClearValue clear_color{};

    clear_color.color.float32[0] = srgb_to_linear(color.r);
    clear_color.color.float32[1] = srgb_to_linear(color.g);
    clear_color.color.float32[2] = srgb_to_linear(color.b);
    clear_color.color.float32[3] = color.a;

    return clear_color;
}

VkClearValue clear_color = make_clear_color_srgb({0.05f, 0.05f, 0.05f, 1.0f});

struct SimpleUniform {
    glm::vec4 color;
};

struct SimpleStorage {
    glm::vec4 color1;
    glm::vec4 color2;
};


std::string to_string(glm::vec3 v) {
    return std::string("(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")");
}

std::string to_string(glm::vec4 v) {
    return std::string("(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")");
}

std::string to_string(glm::quat q)
{
    std::ostringstream ss;
    ss << "quat("
       << "w: " << q.w << ", "
       << "x: " << q.x << ", "
       << "y: " << q.y << ", "
       << "z: " << q.z
       << ")";

    return ss.str();
}

void print_point_cloud(PointCloud& point_cloud, int step = -1) {
    if (step < 0)
        std::cout << "=== Initial values ===" << std::endl;
    else
        std::cout << "=== Step " << step << " ===" << std::endl;
    
    std::cout << "source_point_cloud position: (" << to_string(point_cloud.transform.position) << std::endl;
    std::cout << "source_point_cloud rotation: (" << to_string(point_cloud.transform.rotation) << std::endl;
    std::cout << std::endl;
}


void generate(
    std::vector<PointInstance>& source_points,
    std::vector<PointInstance>& target_points,
    std::vector<glm::vec4>& source_normals,
    std::vector<glm::vec4>& target_normals
)
{
    source_points.clear();
    target_points.clear();
    source_normals.clear();
    target_normals.clear();

    const glm::vec4 source_color = glm::vec4(1, 0, 0, 1);
    const glm::vec4 target_color = glm::vec4(0, 0, 1, 1);

    auto add_point =
        [](
            std::vector<PointInstance>& points,
            std::vector<glm::vec4>& normals,
            const glm::vec3& local_pos,
            const glm::vec3& local_normal,
            const glm::vec4& color,
            const glm::mat4& transform
        )
    {
        PointInstance point;

        point.pos = transform * glm::vec4(local_pos, 1.0f);
        point.color = color;

        glm::vec3 world_normal = glm::normalize(glm::mat3(transform) * local_normal);

        points.push_back(point);
        normals.push_back(glm::vec4(world_normal, 0.0f));
    };

    auto add_grid_face =
        [&](
            std::vector<PointInstance>& points,
            std::vector<glm::vec4>& normals,
            const glm::vec3& origin,
            const glm::vec3& u_axis,
            const glm::vec3& v_axis,
            const glm::vec3& normal,
            int u_steps,
            int v_steps,
            const glm::vec4& color,
            const glm::mat4& transform
        )
    {
        for (int u_i = 0; u_i < u_steps; u_i++)
        {
            float u = float(u_i) / float(u_steps - 1);

            for (int v_i = 0; v_i < v_steps; v_i++)
            {
                float v = float(v_i) / float(v_steps - 1);

                glm::vec3 pos = origin + u_axis * u + v_axis * v;
                add_point(points, normals, pos, normal, color, transform);
            }
        }
    };

    auto steps_for_length = [](float length)
    {
        const float points_per_unit = 12.0f;
        return std::max(2, int(std::round(length * points_per_unit)) + 1);
    };

    auto add_cube =
        [&](
            std::vector<PointInstance>& points,
            std::vector<glm::vec4>& normals,
            const glm::vec3& bottom_center,
            float size,
            const glm::vec4& color,
            const glm::mat4& transform
        )
    {
        glm::vec3 min_corner = bottom_center + glm::vec3(-size * 0.5f, 0.0f, -size * 0.5f);
        glm::vec3 max_corner = bottom_center + glm::vec3( size * 0.5f, size,  size * 0.5f);

        int steps = steps_for_length(size);

        // +X face
        add_grid_face(
            points, normals,
            glm::vec3(max_corner.x, min_corner.y, min_corner.z),
            glm::vec3(0, size, 0),
            glm::vec3(0, 0, size),
            glm::vec3(1, 0, 0),
            steps, steps,
            color, transform
        );

        // -X face
        add_grid_face(
            points, normals,
            glm::vec3(min_corner.x, min_corner.y, min_corner.z),
            glm::vec3(0, size, 0),
            glm::vec3(0, 0, size),
            glm::vec3(-1, 0, 0),
            steps, steps,
            color, transform
        );

        // +Z face
        add_grid_face(
            points, normals,
            glm::vec3(min_corner.x, min_corner.y, max_corner.z),
            glm::vec3(size, 0, 0),
            glm::vec3(0, size, 0),
            glm::vec3(0, 0, 1),
            steps, steps,
            color, transform
        );

        // -Z face
        add_grid_face(
            points, normals,
            glm::vec3(min_corner.x, min_corner.y, min_corner.z),
            glm::vec3(size, 0, 0),
            glm::vec3(0, size, 0),
            glm::vec3(0, 0, -1),
            steps, steps,
            color, transform
        );

        // Top face
        add_grid_face(
            points, normals,
            glm::vec3(min_corner.x, max_corner.y, min_corner.z),
            glm::vec3(size, 0, 0),
            glm::vec3(0, 0, size),
            glm::vec3(0, 1, 0),
            steps, steps,
            color, transform
        );

        // Bottom face skipped because the cube sits on the plane.
    };

    auto add_scene =
        [&](
            std::vector<PointInstance>& points,
            std::vector<glm::vec4>& normals,
            const glm::vec4& color,
            const glm::mat4& transform
        )
    {
        const float plane_length = 24.0f;
        const float plane_width = 5.0f;

        const int plane_length_steps = 240;
        const int plane_width_steps = 50;

        // Long flat plane
        add_grid_face(
            points, normals,
            glm::vec3(-plane_length * 0.5f, 0.0f, -plane_width * 0.5f),
            glm::vec3(plane_length, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, plane_width),
            glm::vec3(0.0f, 1.0f, 0.0f),
            plane_length_steps,
            plane_width_steps,
            color,
            transform
        );

        struct CubeSpec
        {
            glm::vec3 bottom_center;
            float size;
        };

        const std::array<CubeSpec, 6> cubes =
        {{
            { glm::vec3(-8.0f, 0.0f, -1.4f), 1.0f },
            { glm::vec3(-4.5f, 0.0f,  1.2f), 1.3f },
            { glm::vec3(-1.0f, 0.0f, -0.8f), 0.9f },
            { glm::vec3( 3.0f, 0.0f,  1.4f), 1.1f },
            { glm::vec3( 6.5f, 0.0f, -1.1f), 1.5f },
            { glm::vec3( 9.0f, 0.0f,  0.7f), 0.8f },
        }};

        for (const CubeSpec& cube : cubes)
        {
            add_cube(
                points,
                normals,
                cube.bottom_center,
                cube.size,
                color,
                transform
            );
        }
    };

    glm::mat4 source_transform = glm::mat4(1.0f);

    glm::mat4 target_transform = glm::mat4(1.0f);
    target_transform = glm::translate(target_transform, glm::vec3(1.5f, 0.25f, -0.8f));
    target_transform = glm::rotate(target_transform, glm::radians(8.0f), glm::vec3(0, 1, 0));

    add_scene(source_points, source_normals, source_color, source_transform);
    add_scene(target_points, target_normals, target_color, target_transform);
}


int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");

    QueueRequest queue_request;
    queue_request.graphics_count = 1;
    queue_request.present_count = 1;
    queue_request.compute_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    Camera camera;
    FPSCameraController camera_controller(camera);
    camera_controller.speed = 20;

    FrameResources frame_resources(engine.physical_device(), engine.device(), engine.num_frames_in_flight());
    
    VulkanResourceLoader resource_loader(engine, 1024 * 1024 * 100); // 1 Мб

    ShaderManager shader_manager(engine.device());
    TextureManager texture_manager(engine, resource_loader);
    MaterialManager material_manager(engine, shader_manager, frame_resources);
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, material_instance_manager, mesh_manager);

    GICPPass gicp_pass(engine, compute_pass_manager);
    VoxelMapPointInserter voxel_map_inserter(engine, compute_pass_manager);
    VoxelMapPointReseter voxel_map_reseter(engine, compute_pass_manager);

    

    // Compute pass
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VulkanBuffer test_uniform = VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(SimpleUniform));
    // VulkanBuffer test_ssbo = VulkanBuffer::create_storage_buffer(engine, sizeof(SimpleStorage));

    // ComputePassInstance test_instance(compute_pass_manager.descriptor_pool(), compute_pass_manager.test_compute_pass);
    // test_instance.set_uniform_buffer(0, test_uniform);
    // test_instance.set_storage_buffer(1, test_ssbo);

    // VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    // VulkanFence compute_fence(engine.device());
    // {
    //     auto compute_scope = compute_command_buffer.begin_scope();
        
    //     test_instance.bind(compute_command_buffer);
    //     compute_command_buffer.dispatch(1, 1, 1);
    // }

    // engine.compute_submit(compute_command_buffer, &compute_fence);
    // compute_fence.wait();
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Renderer renderer(engine, frame_resources);

    RenderObject unlit_cube(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube2(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube3(mesh_manager.cube, material_instance_manager.rock_blinn_phong);
    RenderObject unlit_cube4(mesh_manager.cube, material_instance_manager.unlit);
    
    std::vector<PointInstance> source_points;
    std::vector<glm::vec4> source_normals;

    std::vector<PointInstance> target_points;
    std::vector<glm::vec4> target_normals;

    generate(source_points, target_points, source_normals, target_normals);

    // const int lat_steps = 100;
    // const int lon_steps = 100;

    // const float radius = 3.0f;

    // glm::vec3 source_center = glm::vec3(0.0f, 0.0f, 0.0f);
    // glm::vec3 target_center = glm::vec3(0.0f, 0.0f, 0.0f);

    // for (int lat = 0; lat < lat_steps; lat++)
    // {
    //     float theta = glm::pi<float>() * float(lat) / float(lat_steps - 1);
    //     // theta: 0 -> pi

    //     for (int lon = 0; lon < lon_steps; lon++)
    //     {
    //         float phi = glm::two_pi<float>() * float(lon) / float(lon_steps);
    //         // phi: 0 -> 2pi

    //         glm::vec3 unit_pos;
    //         unit_pos.x = std::sin(theta) * std::cos(phi);
    //         unit_pos.y = std::cos(theta);
    //         unit_pos.z = std::sin(theta) * std::sin(phi);

    //         glm::vec4 normal = glm::vec4(unit_pos, 0.0f);

    //         {
    //             PointInstance point;
    //             glm::vec3 pos = target_center + unit_pos * radius;

    //             point.pos = glm::vec4(pos, 1.0f);
    //             point.color = glm::vec4(0, 0, 1, 1);

    //             target_points.push_back(point);
    //             target_normals.push_back(normal);
    //         }

    //         {
    //             PointInstance point;
    //             glm::vec3 pos = source_center + unit_pos * radius;

    //             point.pos = glm::vec4(pos, 1.0f);
    //             point.color = glm::vec4(1, 0, 0, 1);

    //             source_points.push_back(point);
    //             source_normals.push_back(normal);
    //         }
    //     }
    // }
    
    VulkanBuffer source_normal_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, source_normals.size() * sizeof(glm::vec4)));
    VulkanBuffer target_normal_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, target_normals.size() * sizeof(glm::vec4)));

    source_normal_buffer.upload(source_normals);
    target_normal_buffer.upload(target_normals);

    PointCloud target_point_cloud(manager_bundle, target_points);
    PointCloud source_point_cloud(manager_bundle, source_points);

    source_point_cloud.transform.position = glm::vec4(1, 1, 1, 1);

    print_point_cloud(source_point_cloud, -1);
    
    // PointCloud point_cloud(manager_bundle, points);
    // LidarScan lidar_scan(manager_bundle, path_utils::executable_dir() / "assets" / "lidar_scans" / "frame_000000.bin");
    LidarVideo lidar_video(manager_bundle, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 0, -1);


    uint32_t test_frame = 0;
    size_t current_frame_id = 0;
    size_t last_inserted_frame_id = current_frame_id;


    // Important: save original first frame pose before overwriting it.
    glm::vec3 first_position = lidar_video.get_scan(0).point_cloud().transform.position;
    glm::quat first_rotation = glm::normalize(lidar_video.get_scan(0).point_cloud().transform.rotation);

    for (int i = static_cast<int>(lidar_video.get_scan_count()) - 1; i >= 1; --i) {
        glm::vec3 p_prev = lidar_video.get_scan(i - 1).point_cloud().transform.position;
        glm::vec3 p_curr = lidar_video.get_scan(i).point_cloud().transform.position;

        glm::quat q_prev = glm::normalize(lidar_video.get_scan(i - 1).point_cloud().transform.rotation);
        glm::quat q_curr = glm::normalize(lidar_video.get_scan(i).point_cloud().transform.rotation);

        // Optional: avoid quaternion sign discontinuity.
        // q and -q represent the same rotation.
        if (glm::dot(q_prev, q_curr) < 0.0f) {
            q_curr = -q_curr;
        }

        glm::vec3 delta_position = p_curr - p_prev;

        // Since your update convention is:
        // q_new = dq * q_old
        glm::quat delta_rotation = glm::normalize(q_curr * glm::inverse(q_prev));

        lidar_video.get_scan(i).point_cloud().transform.position = delta_position;
        lidar_video.get_scan(i).point_cloud().transform.rotation = delta_rotation;
    }

    lidar_video.get_scan(0).point_cloud().transform.position = glm::vec3(0.0f);
    lidar_video.get_scan(0).point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);







    // PointCloud& target_point_cloud = lidar_video.get_scan(0).point_cloud();
    // PointCloud& source_point_cloud = lidar_video.get_scan(4).point_cloud();

    // VulkanBuffer target_normal_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * target_point_cloud.instance_count()));
    // VulkanBuffer source_normal_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * source_point_cloud.instance_count()));

    VoxelPointMap voxel_point_map(engine, 1500000, 1500000);
    voxel_map_reseter.reset(voxel_point_map);

    // voxel_map_inserter.insert(voxel_point_map, target_point_cloud, target_normal_buffer);
    voxel_map_inserter.insert(voxel_point_map, lidar_video.get_scan(0).point_cloud(), lidar_video.get_scan(0).normal_buffer());

    PointCloud voxel_map_point_cloud(manager_bundle, voxel_point_map.map_point_buffer, voxel_point_map.m_map_point_count);

    // voxel_map_point_cloud.instance_data.external_buffer = &voxel_point_map.map_point_buffer;
    // voxel_map_point_cloud.instance_data.external_buffer = &source_point_cloud.instance_data.buffer();

    unlit_cube.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube2.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube3.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube4.set_material_data<UnlitMaterialData>({glm::vec4(0, 1, 1, 1)});

    material_instance_manager.unlit.material_buffer.sync();
    material_instance_manager.rock_blinn_phong.material_buffer.sync();
    material_instance_manager.dirt_blinn_phong.material_buffer.sync();

    unlit_cube.transform.position.x = 0;
    unlit_cube2.transform.position.x = 2;
    unlit_cube3.transform.position.x = 4;
    unlit_cube4.transform.position.x = 6;

    unlit_cube.add_child(unlit_cube2);
    unlit_cube2.add_child(unlit_cube3);
    unlit_cube3.add_child(unlit_cube4);
    // unlit_cube4.add_child(point_cloud);

    Scene scene;

    // lidar_video.get_scan(2).point_cloud().transform.position.y += 4;

    // scene.add(unlit_cube);
    // scene.add(lidar_video);
    scene.add(voxel_map_point_cloud);
    // scene.add(source_point_cloud);
    
    // scene.add(lidar_video);
    // scene.add(lidar_video.get_scan(2).point_cloud());
    // scene.add(source_point_cloud);
    // scene.add(target_point_cloud);
    
    // lidar_video.set_looped(true);
    // scene.add(lidar_scan);

    bool g_pressed = false;
    bool n_pressed = false;

    int step = 0;
    
    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    while (!engine.window().should_close()) {
        engine.window().poll_events();

        float current_frame_time = (float)glfwGetTime() - start_time;
        float delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        timer += delta_time;
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), camera);

        if (!g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_PRESS) {
            g_pressed = true;

            uint32_t current_frame_id = lidar_video.current_frame_id();

            if (current_frame_id > 0) {
                LidarScan& current_scan = lidar_video.get_scan(current_frame_id);



                // auto& pc = current_scan.point_cloud();

                // std::cout << "LidarVideo transform pos: "
                //         << lidar_video.transform.position.x << ", "
                //         << lidar_video.transform.position.y << ", "
                //         << lidar_video.transform.position.z << std::endl;

                // std::cout << "LidarScan transform pos: "
                //         << current_scan.transform.position.x << ", "
                //         << current_scan.transform.position.y << ", "
                //         << current_scan.transform.position.z << std::endl;

                // std::cout << "PointCloud transform pos: "
                //         << pc.transform.position.x << ", "
                //         << pc.transform.position.y << ", "
                //         << pc.transform.position.z << std::endl;

                gicp_pass.step(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
            }

            // gicp_pass.step(voxel_point_map, source_point_cloud, source_normal_buffer);

            // print_point_cloud(source_point_cloud, step);
            step++;
        }

        if (g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_RELEASE) {
            g_pressed = false;
        }

        if (!n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_PRESS) {
            n_pressed = true;
            
            uint32_t current_frame_id = lidar_video.current_frame_id();


            // if (current_frame_id > 0) {
            //     auto& curr = point_cloud_video.frames[current_frame_id].point_cloud;
            //     const auto& prev = point_cloud_video.frames[current_frame_id - 1].point_cloud;

            //     curr.position = prev.position + curr.position;

            //     curr.rotation = glm::normalize(curr.rotation * prev.rotation);
            // }

            if (current_frame_id > 0) {
                LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
                LidarScan& previous_scan = lidar_video.get_scan(current_frame_id - 1);

                PointCloud& current_point_cloud = current_scan.point_cloud();
                PointCloud& previous_point_cloud = previous_scan.point_cloud();

                current_point_cloud.transform.position = previous_point_cloud.transform.position + current_point_cloud.transform.position;

                current_point_cloud.transform.rotation = glm::normalize(current_point_cloud.transform.rotation * previous_point_cloud.transform.rotation);

                gicp_pass.fit(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer(), 10);

                voxel_map_inserter.insert(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
                voxel_map_point_cloud.set_instance_view(voxel_point_map.get_map_instance_view());
            }
            
            lidar_video.next_frame();

            // lidar_video.compose_current_pose_from_previous();
        }

        if (n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_RELEASE) {
            n_pressed = false;

        }

        // unlit_cube.transform.position.x = sin(timer) * 5 ;

        glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f); // Y axis
        float angle = delta_time;
        glm::quat delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube.transform.rotation = delta * unlit_cube.transform.rotation;

        axis = glm::vec3(0.0f, 0.0f, 1.0f); // Y axis
        angle = delta_time;
        delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube3.transform.rotation = delta * unlit_cube3.transform.rotation;

        axis = glm::vec3(1.0f, 0.0f, 0.0f); // Y axis
        angle = sin(timer * 3.14f / 2.0f);
        delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube2.transform.rotation = delta * glm::quat(1.0f, 0.0f, 0.0f, 0.0f);;

        // lidar_video.move(timer);

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                // engine.swapchain_resources().swapchain, {{0.05f, 0.05f, 0.05f, 1.0f}});
                engine.swapchain_resources().swapchain, clear_color);
                // rgba(37, 150, 190)


                renderer.render(command_buffer, scene);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
