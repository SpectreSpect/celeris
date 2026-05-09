#include "gicp_test_clouds.h"


void GICPTestClouds::create_roads(VulkanEngine* engine) {
    generate_road(engine, &target_frame, glm::vec4(0, 0, 1, 1));
    generate_road(engine, &source_frame, glm::vec4(1, 0, 0, 1));

    source_frame.point_cloud.position = glm::vec3(-5, -5, -5);
    source_frame.point_cloud.rotation = glm::vec3(1, -1, 1);


    // source_frame.point_cloud.position += glm::vec3(25, 5, 100);
    // source_frame.point_cloud.rotation += glm::vec3(0, 2, 0);

    // target_frame.point_cloud.position += glm::vec3(25, 5, 100);
    // target_frame.point_cloud.rotation += glm::vec3(0, 2, 0);


    source_frame.point_cloud.position += glm::vec3(0, 0, 1000);
    target_frame.point_cloud.position += glm::vec3(0, 0, 1000);
}

void GICPTestClouds::generate_road(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color) {
    frame->points.clear();
    frame->normals.clear();

    // Road
    float road_length = 80.0f;
    float road_width  = 10.0f;
    float spacing     = 0.35f;

    glm::vec4 road_color = color;
    glm::vec4 road_normal = glm::vec4(0, 1, 0, 0);

    for (float z = -road_length * 0.5f; z <= road_length * 0.5f; z += spacing) {
        for (float x = -road_width * 0.5f; x <= road_width * 0.5f; x += spacing) {
            add_point(
                frame,
                glm::vec4(x, 0.0f, z, 1.0f),
                road_color,
                road_normal
            );
        }
    }

    // Sphere helper
    auto add_sphere = [&](glm::vec3 center, float radius, glm::vec4 color) {
        int rings = 24;
        int sectors = 48;

        for (int r = 0; r <= rings; r++) {
            float v = static_cast<float>(r) / static_cast<float>(rings);
            float phi = v * glm::pi<float>();

            for (int s = 0; s <= sectors; s++) {
                float u = static_cast<float>(s) / static_cast<float>(sectors);
                float theta = u * glm::two_pi<float>();

                float x = std::sin(phi) * std::cos(theta);
                float y = std::cos(phi);
                float z = std::sin(phi) * std::sin(theta);

                glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
                glm::vec3 position = center + normal * radius;

                add_point(
                    frame,
                    glm::vec4(position, 1.0f),
                    color,
                    glm::vec4(normal, 0.0f)
                );
            }
        }
    };

    // Spheres sitting on the road
    add_sphere(glm::vec3(-2.5f, 1.0f, -25.0f), 1.0f, color);
    add_sphere(glm::vec3( 2.5f, 1.5f, -10.0f), 1.5f, color);
    add_sphere(glm::vec3(-1.5f, 0.75f,  8.0f), 0.75f, color);
    add_sphere(glm::vec3( 3.0f, 1.25f, 25.0f), 1.25f, color);

    frame->point_cloud.create(*engine, frame->points.size());
    frame->normal_buffer.create(*engine, frame->points.size() * sizeof(glm::vec4));
    
    frame->normal_buffer.update_data(frame->normals.data(), frame->normals.size() * sizeof(glm::vec4));
    frame->point_cloud.set_points(frame->points);
}

void GICPTestClouds::add_point(PointCloudFrame* frame, const glm::vec4& pos, const glm::vec4& color, const glm::vec4& normal) {
    PointInstance point;
    point.pos = pos;
    point.color = color;

    frame->points.push_back(point);
    frame->normals.push_back(normal);
}