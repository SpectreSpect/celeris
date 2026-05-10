#include "gicp_test_clouds.h"


void GICPTestClouds::create_roads(VulkanEngine* engine) {
    trees_and_spheres(engine, &target_frame, glm::vec4(0, 0, 1, 1), 10000);
    trees_and_spheres(engine, &source_frame, glm::vec4(1, 0, 0, 1), 10000);

    // generate_single_point(engine, &target_frame, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // generate_single_point(engine, &source_frame, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // source_frame.point_cloud.position = glm::vec3(-1, -1, -1);
    // source_frame.point_cloud.position = glm::vec3(-1, -1, 0);
    source_frame.point_cloud.rotation = glm::vec3(0, -0.5, 0);


    // source_frame.point_cloud.position += glm::vec3(25, 5, 100);
    // source_frame.point_cloud.rotation += glm::vec3(0, 2, 0);

    // target_frame.point_cloud.position += glm::vec3(25, 5, 100);
    // target_frame.point_cloud.rotation += glm::vec3(0, 2, 0);

    


    // source_frame.point_cloud.position += glm::vec3(0, 0, 1000);
    // target_frame.point_cloud.position += glm::vec3(0, 0, 1000);
}


void GICPTestClouds::generate_single_point(
    VulkanEngine* engine,
    PointCloudFrame* frame,
    glm::vec4 color,
    const glm::vec3& position,
    const glm::vec3& normal
) {
    frame->points.clear();
    frame->normals.clear();

    frame->points.reserve(1);
    frame->normals.reserve(1);

    glm::vec3 safe_normal = normal;

    if (glm::length(safe_normal) < 0.00001f) {
        safe_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        safe_normal = glm::normalize(safe_normal);
    }

    add_point(
        frame,
        glm::vec4(position, 1.0f),
        color,
        glm::vec4(safe_normal, 0.0f)
    );

    frame->point_cloud.create(*engine, frame->points.size());
    frame->normal_buffer.create(*engine, frame->points.size() * sizeof(glm::vec4));

    frame->normal_buffer.update_data(
        frame->normals.data(),
        frame->normals.size() * sizeof(glm::vec4)
    );

    frame->point_cloud.set_points(frame->points);
}

void GICPTestClouds::generate_road(VulkanEngine* engine, PointCloudFrame* frame, glm::vec4 color, std::size_t target_point_count) {
    frame->points.clear();
    frame->normals.clear();

    frame->points.reserve(target_point_count);
    frame->normals.reserve(target_point_count);

    if (target_point_count == 0) {
        return;
    }

    constexpr float road_length = 80.0f;
    constexpr float road_width  = 10.0f;

    const glm::vec4 road_color  = color;
    const glm::vec4 road_normal = glm::vec4(0, 1, 0, 0);

    struct Surface {
        enum class Type {
            Road,
            Sphere
        };

        Type type;

        glm::vec3 center{};
        float radius = 0.0f;

        float area = 0.0f;
        std::size_t count = 0;
        float remainder = 0.0f;
    };

    std::vector<Surface> surfaces;

    surfaces.push_back({
        .type = Surface::Type::Road,
        .area = road_length * road_width
    });

    auto add_sphere_surface = [&](glm::vec3 center, float radius) {
        surfaces.push_back({
            .type = Surface::Type::Sphere,
            .center = center,
            .radius = radius,
            .area = 4.0f * glm::pi<float>() * radius * radius
        });
    };

    add_sphere_surface(glm::vec3(-2.5f, 1.0f,  -25.0f), 1.0f);
    add_sphere_surface(glm::vec3( 2.5f, 1.5f,  -10.0f), 1.5f);
    add_sphere_surface(glm::vec3(-1.5f, 0.75f,   8.0f), 0.75f);
    add_sphere_surface(glm::vec3( 3.0f, 1.25f,  25.0f), 1.25f);

    float total_area = 0.0f;
    for (const Surface& surface : surfaces) {
        total_area += surface.area;
    }

    std::size_t assigned_count = 0;

    for (Surface& surface : surfaces) {
        float exact_count =
            static_cast<float>(target_point_count) * surface.area / total_area;

        surface.count = static_cast<std::size_t>(std::floor(exact_count));
        surface.remainder = exact_count - static_cast<float>(surface.count);

        assigned_count += surface.count;
    }

    while (assigned_count < target_point_count) {
        auto best = std::max_element(
            surfaces.begin(),
            surfaces.end(),
            [](const Surface& a, const Surface& b) {
                return a.remainder < b.remainder;
            }
        );

        best->count++;
        best->remainder = 0.0f;
        assigned_count++;
    }

    auto add_road_points = [&](std::size_t count) {
        if (count == 0) {
            return;
        }

        constexpr float golden_ratio_conjugate = 0.6180339887498948f;

        for (std::size_t i = 0; i < count; i++) {
            float v = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            float u = glm::fract((static_cast<float>(i) + 0.5f) * golden_ratio_conjugate);

            float x = (u - 0.5f) * road_width;
            float z = (v - 0.5f) * road_length;

            add_point(
                frame,
                glm::vec4(x, 0.0f, z, 1.0f),
                road_color,
                road_normal
            );
        }
    };

    auto add_sphere_points = [&](glm::vec3 center, float radius, std::size_t count) {
        if (count == 0) {
            return;
        }

        const float golden_angle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

        for (std::size_t i = 0; i < count; i++) {
            float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);

            float y = 1.0f - 2.0f * t;
            float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float theta = golden_angle * static_cast<float>(i);

            glm::vec3 normal = glm::normalize(glm::vec3(
                std::cos(theta) * r,
                y,
                std::sin(theta) * r
            ));

            glm::vec3 position = center + normal * radius;

            add_point(
                frame,
                glm::vec4(position, 1.0f),
                color,
                glm::vec4(normal, 0.0f)
            );
        }
    };

    for (const Surface& surface : surfaces) {
        if (surface.type == Surface::Type::Road) {
            add_road_points(surface.count);
        } else {
            add_sphere_points(surface.center, surface.radius, surface.count);
        }
    }

    frame->point_cloud.create(*engine, frame->points.size());
    frame->normal_buffer.create(*engine, frame->points.size() * sizeof(glm::vec4));

    frame->normal_buffer.update_data(
        frame->normals.data(),
        frame->normals.size() * sizeof(glm::vec4)
    );

    frame->point_cloud.set_points(frame->points);
}

void GICPTestClouds::trees_and_spheres(
    VulkanEngine* engine,
    PointCloudFrame* frame,
    glm::vec4 color,
    std::size_t target_point_count
) {
    frame->points.clear();
    frame->normals.clear();

    frame->points.reserve(target_point_count);
    frame->normals.reserve(target_point_count);

    if (target_point_count == 0) {
        return;
    }

    constexpr float scene_length = 80.0f;
    constexpr float corridor_width = 10.0f;

    struct Surface {
        enum class Type {
            Sphere,
            Box
        };

        Type type;

        glm::vec3 center{};
        float radius = 0.0f;

        glm::vec3 half_extents{};

        float area = 0.0f;
        std::size_t count = 0;
        float remainder = 0.0f;
    };

    std::vector<Surface> surfaces;

    auto add_sphere_surface = [&](glm::vec3 center, float radius) {
        surfaces.push_back({
            .type = Surface::Type::Sphere,
            .center = center,
            .radius = radius,
            .area = 4.0f * glm::pi<float>() * radius * radius
        });
    };

    auto add_box_surface = [&](glm::vec3 center, glm::vec3 half_extents) {
        float sx = half_extents.x * 2.0f;
        float sy = half_extents.y * 2.0f;
        float sz = half_extents.z * 2.0f;

        float area = 2.0f * (sx * sy + sx * sz + sy * sz);

        surfaces.push_back({
            .type = Surface::Type::Box,
            .center = center,
            .half_extents = half_extents,
            .area = area
        });
    };

    // Spheres in the middle of the scene
    add_sphere_surface(glm::vec3(-2.5f, 1.0f,  -25.0f), 1.0f);
    add_sphere_surface(glm::vec3( 2.5f, 1.5f,  -10.0f), 1.5f);
    add_sphere_surface(glm::vec3(-1.5f, 0.75f,   8.0f), 0.75f);
    add_sphere_surface(glm::vec3( 3.0f, 1.25f,  25.0f), 1.25f);

    // Rows of rectangular "trees" on both sides
    constexpr float tree_width  = 0.8f;
    constexpr float tree_depth  = 0.8f;
    constexpr float tree_height = 5.0f;

    constexpr float tree_spacing = 8.0f;
    constexpr float tree_x_offset = corridor_width * 0.5f + 3.0f;

    glm::vec3 tree_half_extents(
        tree_width  * 0.5f,
        tree_height * 0.5f,
        tree_depth  * 0.5f
    );

    for (float z = -scene_length * 0.5f + 5.0f;
         z <= scene_length * 0.5f - 5.0f;
         z += tree_spacing)
    {
        add_box_surface(
            glm::vec3(-tree_x_offset, tree_height * 0.5f, z),
            tree_half_extents
        );

        add_box_surface(
            glm::vec3(tree_x_offset, tree_height * 0.5f, z),
            tree_half_extents
        );
    }

    float total_area = 0.0f;
    for (const Surface& surface : surfaces) {
        total_area += surface.area;
    }

    std::size_t assigned_count = 0;

    for (Surface& surface : surfaces) {
        float exact_count =
            static_cast<float>(target_point_count) * surface.area / total_area;

        surface.count = static_cast<std::size_t>(std::floor(exact_count));
        surface.remainder = exact_count - static_cast<float>(surface.count);

        assigned_count += surface.count;
    }

    while (assigned_count < target_point_count) {
        auto best = std::max_element(
            surfaces.begin(),
            surfaces.end(),
            [](const Surface& a, const Surface& b) {
                return a.remainder < b.remainder;
            }
        );

        best->count++;
        best->remainder = 0.0f;
        assigned_count++;
    }

    auto add_sphere_points = [&](glm::vec3 center, float radius, std::size_t count) {
        if (count == 0) {
            return;
        }

        const float golden_angle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

        for (std::size_t i = 0; i < count; i++) {
            float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);

            float y = 1.0f - 2.0f * t;
            float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float theta = golden_angle * static_cast<float>(i);

            glm::vec3 normal = glm::normalize(glm::vec3(
                std::cos(theta) * r,
                y,
                std::sin(theta) * r
            ));

            glm::vec3 position = center + normal * radius;

            add_point(
                frame,
                glm::vec4(position, 1.0f),
                color,
                glm::vec4(normal, 0.0f)
            );
        }
    };

    auto add_box_points = [&](glm::vec3 center, glm::vec3 half_extents, std::size_t count) {
        if (count == 0) {
            return;
        }

        struct Face {
            glm::vec3 normal{};
            int fixed_axis = 0;
            float fixed_value = 0.0f;

            int u_axis = 1;
            int v_axis = 2;

            float u_half = 0.0f;
            float v_half = 0.0f;

            float area = 0.0f;
            std::size_t count = 0;
            float remainder = 0.0f;
        };

        Face faces[6] = {
            {
                .normal = glm::vec3( 1, 0, 0),
                .fixed_axis = 0,
                .fixed_value =  half_extents.x,
                .u_axis = 1,
                .v_axis = 2,
                .u_half = half_extents.y,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.y * half_extents.z
            },
            {
                .normal = glm::vec3(-1, 0, 0),
                .fixed_axis = 0,
                .fixed_value = -half_extents.x,
                .u_axis = 1,
                .v_axis = 2,
                .u_half = half_extents.y,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.y * half_extents.z
            },
            {
                .normal = glm::vec3(0,  1, 0),
                .fixed_axis = 1,
                .fixed_value =  half_extents.y,
                .u_axis = 0,
                .v_axis = 2,
                .u_half = half_extents.x,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.x * half_extents.z
            },
            {
                .normal = glm::vec3(0, -1, 0),
                .fixed_axis = 1,
                .fixed_value = -half_extents.y,
                .u_axis = 0,
                .v_axis = 2,
                .u_half = half_extents.x,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.x * half_extents.z
            },
            {
                .normal = glm::vec3(0, 0,  1),
                .fixed_axis = 2,
                .fixed_value =  half_extents.z,
                .u_axis = 0,
                .v_axis = 1,
                .u_half = half_extents.x,
                .v_half = half_extents.y,
                .area = 4.0f * half_extents.x * half_extents.y
            },
            {
                .normal = glm::vec3(0, 0, -1),
                .fixed_axis = 2,
                .fixed_value = -half_extents.z,
                .u_axis = 0,
                .v_axis = 1,
                .u_half = half_extents.x,
                .v_half = half_extents.y,
                .area = 4.0f * half_extents.x * half_extents.y
            }
        };

        float total_face_area = 0.0f;
        for (const Face& face : faces) {
            total_face_area += face.area;
        }

        std::size_t assigned_face_count = 0;

        for (Face& face : faces) {
            float exact_count =
                static_cast<float>(count) * face.area / total_face_area;

            face.count = static_cast<std::size_t>(std::floor(exact_count));
            face.remainder = exact_count - static_cast<float>(face.count);

            assigned_face_count += face.count;
        }

        while (assigned_face_count < count) {
            Face* best = &faces[0];

            for (Face& face : faces) {
                if (face.remainder > best->remainder) {
                    best = &face;
                }
            }

            best->count++;
            best->remainder = 0.0f;
            assigned_face_count++;
        }

        constexpr float golden_ratio_conjugate = 0.6180339887498948f;

        for (const Face& face : faces) {
            for (std::size_t i = 0; i < face.count; i++) {
                float a = glm::fract(
                    (static_cast<float>(i) + 0.5f) * golden_ratio_conjugate
                );

                float b =
                    (static_cast<float>(i) + 0.5f) /
                    static_cast<float>(face.count);

                glm::vec3 local(0.0f);

                local[face.fixed_axis] = face.fixed_value;
                local[face.u_axis] = (a - 0.5f) * 2.0f * face.u_half;
                local[face.v_axis] = (b - 0.5f) * 2.0f * face.v_half;

                glm::vec3 position = center + local;

                add_point(
                    frame,
                    glm::vec4(position, 1.0f),
                    color,
                    glm::vec4(face.normal, 0.0f)
                );
            }
        }
    };

    for (const Surface& surface : surfaces) {
        if (surface.type == Surface::Type::Sphere) {
            add_sphere_points(surface.center, surface.radius, surface.count);
        } else if (surface.type == Surface::Type::Box) {
            add_box_points(surface.center, surface.half_extents, surface.count);
        }
    }

    frame->point_cloud.create(*engine, frame->points.size());
    frame->normal_buffer.create(*engine, frame->points.size() * sizeof(glm::vec4));

    frame->normal_buffer.update_data(
        frame->normals.data(),
        frame->normals.size() * sizeof(glm::vec4)
    );

    frame->point_cloud.set_points(frame->points);
}

void GICPTestClouds::road_with_trees(
    VulkanEngine* engine,
    PointCloudFrame* frame,
    glm::vec4 color,
    std::size_t target_point_count
) {
    frame->points.clear();
    frame->normals.clear();

    frame->points.reserve(target_point_count);
    frame->normals.reserve(target_point_count);

    if (target_point_count == 0) {
        return;
    }

    constexpr float road_length = 80.0f;
    constexpr float road_width  = 10.0f;

    const glm::vec4 road_color  = color;
    const glm::vec4 road_normal = glm::vec4(0, 1, 0, 0);

    struct Surface {
        enum class Type {
            Road,
            Sphere,
            Box
        };

        Type type;

        glm::vec3 center{};
        float radius = 0.0f;

        glm::vec3 half_extents{};

        float area = 0.0f;
        std::size_t count = 0;
        float remainder = 0.0f;
    };

    std::vector<Surface> surfaces;

    surfaces.push_back({
        .type = Surface::Type::Road,
        .area = road_length * road_width
    });

    auto add_sphere_surface = [&](glm::vec3 center, float radius) {
        surfaces.push_back({
            .type = Surface::Type::Sphere,
            .center = center,
            .radius = radius,
            .area = 4.0f * glm::pi<float>() * radius * radius
        });
    };

    auto add_box_surface = [&](glm::vec3 center, glm::vec3 half_extents) {
        float sx = half_extents.x * 2.0f;
        float sy = half_extents.y * 2.0f;
        float sz = half_extents.z * 2.0f;

        float area = 2.0f * (sx * sy + sx * sz + sy * sz);

        surfaces.push_back({
            .type = Surface::Type::Box,
            .center = center,
            .half_extents = half_extents,
            .area = area
        });
    };

    // Spheres sitting on the road
    add_sphere_surface(glm::vec3(-2.5f, 1.0f,  -25.0f), 1.0f);
    add_sphere_surface(glm::vec3( 2.5f, 1.5f,  -10.0f), 1.5f);
    add_sphere_surface(glm::vec3(-1.5f, 0.75f,   8.0f), 0.75f);
    add_sphere_surface(glm::vec3( 3.0f, 1.25f,  25.0f), 1.25f);

    // Rows of rectangular "trees" on both sides of the road
    constexpr float tree_width  = 0.8f;
    constexpr float tree_depth  = 0.8f;
    constexpr float tree_height = 5.0f;

    constexpr float tree_spacing = 8.0f;
    constexpr float tree_x_offset = road_width * 0.5f + 3.0f;

    glm::vec3 tree_half_extents(
        tree_width  * 0.5f,
        tree_height * 0.5f,
        tree_depth  * 0.5f
    );

    for (float z = -road_length * 0.5f + 5.0f;
         z <= road_length * 0.5f - 5.0f;
         z += tree_spacing)
    {
        add_box_surface(
            glm::vec3(-tree_x_offset, tree_height * 0.5f, z),
            tree_half_extents
        );

        add_box_surface(
            glm::vec3(tree_x_offset, tree_height * 0.5f, z),
            tree_half_extents
        );
    }

    float total_area = 0.0f;
    for (const Surface& surface : surfaces) {
        total_area += surface.area;
    }

    std::size_t assigned_count = 0;

    for (Surface& surface : surfaces) {
        float exact_count =
            static_cast<float>(target_point_count) * surface.area / total_area;

        surface.count = static_cast<std::size_t>(std::floor(exact_count));
        surface.remainder = exact_count - static_cast<float>(surface.count);

        assigned_count += surface.count;
    }

    while (assigned_count < target_point_count) {
        auto best = std::max_element(
            surfaces.begin(),
            surfaces.end(),
            [](const Surface& a, const Surface& b) {
                return a.remainder < b.remainder;
            }
        );

        best->count++;
        best->remainder = 0.0f;
        assigned_count++;
    }

    auto add_road_points = [&](std::size_t count) {
        if (count == 0) {
            return;
        }

        constexpr float golden_ratio_conjugate = 0.6180339887498948f;

        for (std::size_t i = 0; i < count; i++) {
            float v = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            float u = glm::fract(
                (static_cast<float>(i) + 0.5f) * golden_ratio_conjugate
            );

            float x = (u - 0.5f) * road_width;
            float z = (v - 0.5f) * road_length;

            add_point(
                frame,
                glm::vec4(x, 0.0f, z, 1.0f),
                road_color,
                road_normal
            );
        }
    };

    auto add_sphere_points = [&](glm::vec3 center, float radius, std::size_t count) {
        if (count == 0) {
            return;
        }

        const float golden_angle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

        for (std::size_t i = 0; i < count; i++) {
            float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);

            float y = 1.0f - 2.0f * t;
            float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float theta = golden_angle * static_cast<float>(i);

            glm::vec3 normal = glm::normalize(glm::vec3(
                std::cos(theta) * r,
                y,
                std::sin(theta) * r
            ));

            glm::vec3 position = center + normal * radius;

            add_point(
                frame,
                glm::vec4(position, 1.0f),
                color,
                glm::vec4(normal, 0.0f)
            );
        }
    };

    auto add_box_points = [&](glm::vec3 center, glm::vec3 half_extents, std::size_t count) {
        if (count == 0) {
            return;
        }

        struct Face {
            glm::vec3 normal{};
            int fixed_axis = 0;
            float fixed_value = 0.0f;

            int u_axis = 1;
            int v_axis = 2;

            float u_half = 0.0f;
            float v_half = 0.0f;

            float area = 0.0f;
            std::size_t count = 0;
            float remainder = 0.0f;
        };

        Face faces[6] = {
            {
                .normal = glm::vec3( 1, 0, 0),
                .fixed_axis = 0,
                .fixed_value =  half_extents.x,
                .u_axis = 1,
                .v_axis = 2,
                .u_half = half_extents.y,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.y * half_extents.z
            },
            {
                .normal = glm::vec3(-1, 0, 0),
                .fixed_axis = 0,
                .fixed_value = -half_extents.x,
                .u_axis = 1,
                .v_axis = 2,
                .u_half = half_extents.y,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.y * half_extents.z
            },
            {
                .normal = glm::vec3(0,  1, 0),
                .fixed_axis = 1,
                .fixed_value =  half_extents.y,
                .u_axis = 0,
                .v_axis = 2,
                .u_half = half_extents.x,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.x * half_extents.z
            },
            {
                .normal = glm::vec3(0, -1, 0),
                .fixed_axis = 1,
                .fixed_value = -half_extents.y,
                .u_axis = 0,
                .v_axis = 2,
                .u_half = half_extents.x,
                .v_half = half_extents.z,
                .area = 4.0f * half_extents.x * half_extents.z
            },
            {
                .normal = glm::vec3(0, 0,  1),
                .fixed_axis = 2,
                .fixed_value =  half_extents.z,
                .u_axis = 0,
                .v_axis = 1,
                .u_half = half_extents.x,
                .v_half = half_extents.y,
                .area = 4.0f * half_extents.x * half_extents.y
            },
            {
                .normal = glm::vec3(0, 0, -1),
                .fixed_axis = 2,
                .fixed_value = -half_extents.z,
                .u_axis = 0,
                .v_axis = 1,
                .u_half = half_extents.x,
                .v_half = half_extents.y,
                .area = 4.0f * half_extents.x * half_extents.y
            }
        };

        float total_face_area = 0.0f;
        for (const Face& face : faces) {
            total_face_area += face.area;
        }

        std::size_t assigned_face_count = 0;

        for (Face& face : faces) {
            float exact_count =
                static_cast<float>(count) * face.area / total_face_area;

            face.count = static_cast<std::size_t>(std::floor(exact_count));
            face.remainder = exact_count - static_cast<float>(face.count);

            assigned_face_count += face.count;
        }

        while (assigned_face_count < count) {
            Face* best = &faces[0];

            for (Face& face : faces) {
                if (face.remainder > best->remainder) {
                    best = &face;
                }
            }

            best->count++;
            best->remainder = 0.0f;
            assigned_face_count++;
        }

        constexpr float golden_ratio_conjugate = 0.6180339887498948f;

        for (const Face& face : faces) {
            for (std::size_t i = 0; i < face.count; i++) {
                float a = glm::fract(
                    (static_cast<float>(i) + 0.5f) * golden_ratio_conjugate
                );

                float b =
                    (static_cast<float>(i) + 0.5f) /
                    static_cast<float>(face.count);

                glm::vec3 local(0.0f);

                local[face.fixed_axis] = face.fixed_value;
                local[face.u_axis] = (a - 0.5f) * 2.0f * face.u_half;
                local[face.v_axis] = (b - 0.5f) * 2.0f * face.v_half;

                glm::vec3 position = center + local;

                add_point(
                    frame,
                    glm::vec4(position, 1.0f),
                    color,
                    glm::vec4(face.normal, 0.0f)
                );
            }
        }
    };

    for (const Surface& surface : surfaces) {
        if (surface.type == Surface::Type::Road) {
            add_road_points(surface.count);
        } else if (surface.type == Surface::Type::Sphere) {
            add_sphere_points(surface.center, surface.radius, surface.count);
        } else if (surface.type == Surface::Type::Box) {
            add_box_points(surface.center, surface.half_extents, surface.count);
        }
    }

    frame->point_cloud.create(*engine, frame->points.size());
    frame->normal_buffer.create(*engine, frame->points.size() * sizeof(glm::vec4));

    frame->normal_buffer.update_data(
        frame->normals.data(),
        frame->normals.size() * sizeof(glm::vec4)
    );

    frame->point_cloud.set_points(frame->points);
}

void GICPTestClouds::add_point(PointCloudFrame* frame, const glm::vec4& pos, const glm::vec4& color, const glm::vec4& normal) {
    PointInstance point;
    point.pos = pos;
    point.color = color;

    frame->points.push_back(point);
    frame->normals.push_back(normal);
}