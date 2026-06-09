#include "point_cloud.h"


PointCloud::PointCloud() {
    mesh_generation_shader = ComputeShader((executable_dir() / "shaders" / "point_cloud_to_mesh" / "generate_mesh_cs.glsl").string());
    mesh_generation_program = ComputeProgram(&mesh_generation_shader);
}

void PointCloud::update_points(std::vector<PointInstance> points) {
    this->points = std::move(points);
    needs_gpu_sync = true;
}

void PointCloud::set_point(unsigned int id, PointInstance value) {
    if (id >= points.size()) {
        std::out_of_range("PointCloud::set_point");
        return;
    }
    
    points[id] = value;
    needs_gpu_sync = true;
}

PointInstance PointCloud::get_point(unsigned int id) {
    if (id >= points.size())
        std::out_of_range("PointCloud::set_point");

    return points[id];
}

void PointCloud::sync_gpu() {
    if(!needs_gpu_sync) return;
    point_renderer.set_points(points);
    needs_gpu_sync = false;
}

size_t PointCloud::size() {
    return points.size();
}

void PointCloud::push_point(std::vector<float>& vertices, const PointInstance& point, const glm::vec3& color, const glm::vec3& normal) {
        vertices.push_back(point.pos.x);
        vertices.push_back(point.pos.y);
        vertices.push_back(point.pos.z);
        vertices.push_back(1.0f);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
        vertices.push_back(1.0f);
        vertices.push_back(color.x);
        vertices.push_back(color.y);
        vertices.push_back(color.z);
        vertices.push_back(1.0f);
}

bool PointCloud::is_point_valid(const PointInstance &p) {
    return std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && std::isfinite(p.pos.z);
}

float PointCloud::radial_distance(const PointInstance &p) {
    return std::hypot(static_cast<double>(p.pos.x), static_cast<double>(p.pos.y), static_cast<double>(p.pos.z));
}

bool PointCloud::is_same_object(const PointInstance &p0, const PointInstance &p1,
                                float rel_thresh, bool more_permissive_with_distance,float abs_thresh)
{
    if (!is_point_valid(p0) || !is_point_valid(p1))
        return false;

    float r0 = radial_distance(p0);
    float r1 = radial_distance(p1);

    if (!std::isfinite(r0) || !std::isfinite(r1))
        return false;

    float allowed = 0;
    float dr = std::fabs(r0 - r1);


    if (more_permissive_with_distance) {
        // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
        // allowed = std::max(thresh * std::min(r0, r1), abs_thresh);
        float permission_factor = 1.5;
        allowed = rel_thresh * pow((std::min(r0, r1) / std::pow(permission_factor, 1.5)), permission_factor);
    }
    else {
        // float thresh = std::max(0.2f - p0.pos.y, 0.0f);
        // allowed = std::max(thresh, abs_thresh);
        allowed = std::max(rel_thresh, abs_thresh);
    }
    return dr <= allowed;
}

glm::vec3 PointCloud::triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c) {
    glm::vec3 av = {a.pos.x, a.pos.y, a.pos.z};
    glm::vec3 bv = {b.pos.x, b.pos.y, b.pos.z};
    glm::vec3 cv = {c.pos.x, c.pos.y, c.pos.z};

    glm::vec3 u = bv - av;
    glm::vec3 v = cv - av;
    glm::vec3 n = glm::cross(u, v);           // unnormalized normal (also proportional to triangle area)
    return glm::normalize(n);       // returns {0,0,0} if degenerate
}

int PointCloud::xy_id(int x, int y, int ring_width, int cloud_size){
    int idx = x + y * ring_width;

    if (idx < 0 || idx >= cloud_size) {
        throw "Bad index";
        return -1;
    }

    return idx;
}

Mesh PointCloud::generate_mesh(float rel_thresh) {
    VertexLayout* vertex_layout = new VertexLayout();
    // vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    // vertex_layout->add("normal", 1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    // vertex_layout->add("color", 2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});
    vertex_layout->add("position", 0, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f, 1.0f});
    vertex_layout->add("normal", 1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 4 * sizeof(float), 0, {0.0f, 1.0f, 0.0f, 1.0f});
    vertex_layout->add("color", 2, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 8 * sizeof(float), 0, {1.0f, 1.0f, 1.0f, 1.0f});

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    int rings_count = 16;
    int ring_width = points.size() / rings_count;
    int cloud_size = points.size();
    
    for (int y = 0; y < rings_count - 1; y++){
        bool top_jump = false;
        bool bottom_jump = false;

        // if (y != 14)
        //     continue;

        for (int x = 0; x < ring_width - 1; x++){
            int id1 = xy_id(x, y, ring_width, cloud_size);
            int id2 = xy_id(x, y + 1, ring_width, cloud_size);
            int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);

            int id4 = xy_id(x + 1, y + 1, ring_width, cloud_size);
            int id5 = xy_id(x + 1, y, ring_width, cloud_size);
            int id6 = xy_id(x, y, ring_width, cloud_size);

            


            // float top_color = get_color_float(WHITE_ID, num_colors);
            // float bottom_color = get_color_float(WHITE_ID, num_colors);

            //Point positions:
            //1 2
            //0 3

            PointInstance p0 = points[id1]; // lower-left
            PointInstance p1 = points[id2]; // upper-left
            PointInstance p2 = points[id3]; // upper-right
            PointInstance p3 = points[id5]; // lower-right


            if (x == 0  && y == 0) {
                std::cout << p0.pos.x << " " << p0.pos.y << " " << p0.pos.z << std::endl;
                std::cout << p1.pos.x << " " << p1.pos.y << " " << p1.pos.z << std::endl;
                std::cout << p2.pos.x << " " << p2.pos.y << " " << p2.pos.z << std::endl;
                std::cout << std::endl;

                std::cout << p0.color.x << " " << p0.color.y << " " << p0.color.z << std::endl;
                std::cout << p1.color.x << " " << p1.color.y << " " << p1.color.z << std::endl;
                std::cout << p2.color.x << " " << p2.color.y << " " << p2.color.z << std::endl;
                std::cout << std::endl;

                // std::cout << p0.time << std::endl;
                // std::cout << p1.time << std::endl;
                // std::cout << p2.time << std::endl;
                // std::cout << std::endl;

                // std::cout << p0.gps_pos.x << " " << p0.gps_pos.y << " " << p0.gps_pos.z << std::endl;
                // std::cout << p1.gps_pos.x << " " << p1.gps_pos.y << " " << p1.gps_pos.z << std::endl;
                // std::cout << p2.gps_pos.x << " " << p2.gps_pos.y << " " << p2.gps_pos.z << std::endl;
                // std::cout << std::endl;

                // std::cout << p0.imu_rotation.x << " " << p0.imu_rotation.y << " " << p0.imu_rotation.z << std::endl;
                // std::cout << p1.imu_rotation.x << " " << p1.imu_rotation.y << " " << p1.imu_rotation.z << std::endl;
                // std::cout << p2.imu_rotation.x << " " << p2.imu_rotation.y << " " << p2.imu_rotation.z << std::endl;


                // glm::vec3 gps_pos;
                // glm::vec3 imu_rotation;
            }

            bool tri1_ok = false;
            bool tri2_ok = false;

            if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
                if (is_same_object(p0, p1, rel_thresh) && is_same_object(p1, p2, 1.5, false))
                    tri1_ok = true;
            }

            if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
                if (is_same_object(p3, p2, rel_thresh) && is_same_object(p3, p0, 1.5, false))
                    tri2_ok = true;
            }

            glm::vec3 surface_normal = triangle_normal(p2, p0, p1);
            glm::vec3 upward_normal = glm::vec3(0.0f, 1.0f, 0.0f);

            // std::cout << surface_normal.x << " " << surface_normal.y << " " << surface_normal.z << " " << std::endl;

            float normal_cos = glm::dot(surface_normal, upward_normal);

            if (tri1_ok && tri2_ok) {
                // glm::vec3 red = {1.0f, 0.0f, 0.0f};
                glm::vec3 white = {1.0f, 1.0f, 1.0f};
                // float t = glm::clamp(std::abs(normal_cos), 0.0f, 1.0f);
                // glm::vec3 color = red + (white - red) * t;
                glm::vec3 color = white;
                
                int last_id = static_cast<unsigned int>(vertices.size() / 12);
                push_point(vertices, p0, color, surface_normal);
                push_point(vertices, p1, color, surface_normal);
                push_point(vertices, p2, color, surface_normal);
                push_point(vertices, p3, color, surface_normal);

                indices.push_back(last_id);
                indices.push_back(last_id+1);
                indices.push_back(last_id+2);

                indices.push_back(last_id+2);
                indices.push_back(last_id+3);
                indices.push_back(last_id);

            }
        }
    }

    Mesh mesh = Mesh(vertices, indices, vertex_layout);
    return mesh;
}

// void PointCloud::get_normals(const std::vector<PointInstance>& points,
//                              std::vector<glm::vec3>& normals)
// {
//     normals.clear();
//     normals.resize(points.size(), glm::vec3(0.0f));

//     if (points.empty())
//         return;

//     const int rings_count = 16;
//     const int cloud_size = static_cast<int>(points.size());

//     if (cloud_size < rings_count)
//         return;

//     const int ring_width = cloud_size / rings_count;
//     if (ring_width < 2)
//         return;

//     const float rel_thresh = 1.0f;

//     auto accumulate_triangle_normal = [&](int ia, int ib, int ic)
//     {
//         const PointInstance& a = points[ia];
//         const PointInstance& b = points[ib];
//         const PointInstance& c = points[ic];

//         // Extra safety: don't accumulate into invalid points
//         if (!is_point_valid(a) || !is_point_valid(b) || !is_point_valid(c))
//             return;

//         glm::vec3 n = triangle_normal(a, b, c);

//         if (glm::dot(n, n) < 1e-12f)
//             return;

//         // Keep normals consistently oriented upward
//         if (glm::dot(n, glm::vec3(0.0f, 1.0f, 0.0f)) < 0.0f)
//             n = -n;

//         normals[ia] += n;
//         normals[ib] += n;
//         normals[ic] += n;
//     };

//     for (int y = 0; y < rings_count - 1; y++) {
//         for (int x = 0; x < ring_width - 1; x++) {
//             int id1 = xy_id(x,     y,     ring_width, cloud_size);
//             int id2 = xy_id(x,     y + 1, ring_width, cloud_size);
//             int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);
//             int id5 = xy_id(x + 1, y,     ring_width, cloud_size);

//             const PointInstance& p0 = points[id1]; // lower-left
//             const PointInstance& p1 = points[id2]; // upper-left
//             const PointInstance& p2 = points[id3]; // upper-right
//             const PointInstance& p3 = points[id5]; // lower-right

//             bool tri1_ok = false;
//             bool tri2_ok = false;

//             if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
//                 if (is_same_object(p0, p1, rel_thresh) &&
//                     is_same_object(p1, p2, 1.5f, false))
//                 {
//                     tri1_ok = true;
//                 }
//             }

//             if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
//                 if (is_same_object(p3, p2, rel_thresh) &&
//                     is_same_object(p3, p0, 1.5f, false))
//                 {
//                     tri2_ok = true;
//                 }
//             }

//             if (tri1_ok) {
//                 accumulate_triangle_normal(id3, id1, id2);
//             }

//             if (tri2_ok) {
//                 accumulate_triangle_normal(id1, id3, id5);
//             }
//         }
//     }

//     // Normalize only valid points that actually accumulated something.
//     // Invalid points remain degenerate: (0,0,0).
//     for (size_t i = 0; i < normals.size(); i++) {
//         if (!is_point_valid(points[i])) {
//             normals[i] = glm::vec3(0.0f);
//             continue;
//         }

//         float len2 = glm::dot(normals[i], normals[i]);
//         if (len2 < 1e-12f) {
//             normals[i] = glm::vec3(0.0f);
//         } else {
//             normals[i] = glm::normalize(normals[i]);
//         }
//     }
// }


void PointCloud::get_normals(const std::vector<PointInstance>& points,
                             std::vector<glm::vec4>& normals)
{
    normals.clear();
    normals.resize(points.size(), glm::vec4(0.0f));

    if (points.empty())
        return;

    const int rings_count = 16;
    const int cloud_size = static_cast<int>(points.size());

    if (cloud_size < rings_count)
        return;

    const int ring_width = cloud_size / rings_count;
    if (ring_width < 2)
        return;

    const float rel_thresh = 1.0f;

    auto accumulate_triangle_normal = [&](int ia, int ib, int ic)
    {
        const PointInstance& a = points[ia];
        const PointInstance& b = points[ib];
        const PointInstance& c = points[ic];

        // Extra safety: don't accumulate into invalid points
        if (!is_point_valid(a) || !is_point_valid(b) || !is_point_valid(c))
            return;

        glm::vec3 n = triangle_normal(a, b, c);

        if (glm::dot(n, n) < 1e-12f)
            return;

        // Keep normals consistently oriented upward
        if (glm::dot(n, glm::vec3(0.0f, 1.0f, 0.0f)) < 0.0f)
            n = -n;

        glm::vec4 n4(n, 0.0f);

        normals[ia] += n4;
        normals[ib] += n4;
        normals[ic] += n4;
    };

    for (int y = 0; y < rings_count - 1; y++) {
        for (int x = 0; x < ring_width - 1; x++) {
            int id1 = xy_id(x,     y,     ring_width, cloud_size);
            int id2 = xy_id(x,     y + 1, ring_width, cloud_size);
            int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);
            int id5 = xy_id(x + 1, y,     ring_width, cloud_size);

            const PointInstance& p0 = points[id1]; // lower-left
            const PointInstance& p1 = points[id2]; // upper-left
            const PointInstance& p2 = points[id3]; // upper-right
            const PointInstance& p3 = points[id5]; // lower-right

            bool tri1_ok = false;
            bool tri2_ok = false;

            if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
                if (is_same_object(p0, p1, rel_thresh) &&
                    is_same_object(p1, p2, 1.5f, false))
                {
                    tri1_ok = true;
                }
            }

            if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
                if (is_same_object(p3, p2, rel_thresh) &&
                    is_same_object(p3, p0, 1.5f, false))
                {
                    tri2_ok = true;
                }
            }

            if (tri1_ok) {
                accumulate_triangle_normal(id3, id1, id2);
            }

            if (tri2_ok) {
                accumulate_triangle_normal(id1, id3, id5);
            }
        }
    }

    // Normalize only valid points that actually accumulated something.
    // Invalid points remain degenerate: (0,0,0,0).
    for (size_t i = 0; i < normals.size(); i++) {
        if (!is_point_valid(points[i])) {
            normals[i] = glm::vec4(0.0f);
            continue;
        }

        glm::vec3 n = glm::vec3(normals[i]);
        float len2 = glm::dot(n, n);

        if (len2 < 1e-12f) {
            normals[i] = glm::vec4(0.0f);
        } else {
            normals[i] = glm::vec4(glm::normalize(n), 0.0f);
        }
    }
}

void PointCloud::get_normals_ssbo(std::vector<glm::vec4> &normals, SSBO& normals_ssbo) {
    normals_ssbo = SSBO(normals.size() * sizeof(glm::vec4), GL_DYNAMIC_DRAW, normals.data());
}

void PointCloud::remove_invalid_points_and_normals(std::vector<PointInstance>& points,
                                                   std::vector<glm::vec4>& normals)
{
    if (points.size() != normals.size()) {
        std::cout << "remove_invalid_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec4> filtered_normals;

    filtered_points.reserve(points.size());
    filtered_normals.reserve(normals.size());

    for (size_t i = 0; i < points.size(); i++) {
        const PointInstance& p = points[i];
        const glm::vec4& n4 = normals[i];
        glm::vec3 n = glm::vec3(n4);

        bool point_valid = is_point_valid(p);
        bool normal_valid = glm::dot(n, n) > 1e-12f;

        if (!point_valid || !normal_valid)
            continue;

        filtered_points.push_back(p);
        filtered_normals.push_back(glm::vec4(glm::normalize(n), 0.0f));
    }

    points = std::move(filtered_points);
    normals = std::move(filtered_normals);
}

void PointCloud::remove_points_near_origin(std::vector<PointInstance>& points,
                                           std::vector<glm::vec4>& normals,
                                           float min_distance)
{
    if (points.size() != normals.size()) {
        std::cout << "remove_points_near_origin: points.size() != normals.size()\n";
        return;
    }

    float min_dist_sq = min_distance * min_distance;

    std::vector<PointInstance> filtered_points;
    std::vector<glm::vec4> filtered_normals;

    filtered_points.reserve(points.size());
    filtered_normals.reserve(normals.size());

    for (size_t i = 0; i < points.size(); i++) {
        const PointInstance& p = points[i];
        const glm::vec4& n = normals[i];

        glm::vec3 pos = glm::vec3(p.pos);
        float dist_sq = glm::dot(pos, pos);

        // remove points that are too close to the lidar origin
        // if (dist_sq < min_dist_sq || p.pos.y < 1.0)
        if (dist_sq < min_dist_sq)
            continue;

        filtered_points.push_back(p);
        filtered_normals.push_back(n);
    }

    points = std::move(filtered_points);
    normals = std::move(filtered_normals);
}

void PointCloud::drop_out_points_and_normals(std::vector<PointInstance>& points,
                                             std::vector<glm::vec4>& normals,
                                             size_t target_size)
{
    if (points.size() != normals.size()) {
        std::cout << "drop_out_points_and_normals: points.size() != normals.size()\n";
        return;
    }

    size_t n = points.size();

    if (target_size >= n) {
        return; // nothing to drop
    }

    if (target_size == 0) {
        points.clear();
        normals.clear();
        return;
    }

    // Create indices [0, 1, 2, ..., n-1]
    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);

    // Shuffle indices randomly
    static std::random_device rd;
    static std::mt19937 rng(rd());
    std::shuffle(indices.begin(), indices.end(), rng);

    // Keep only the first target_size indices
    indices.resize(target_size);

    // Optional: sort so the remaining points keep their original relative order
    std::sort(indices.begin(), indices.end());

    std::vector<PointInstance> new_points;
    std::vector<glm::vec4> new_normals;

    new_points.reserve(target_size);
    new_normals.reserve(target_size);

    for (size_t idx : indices) {
        new_points.push_back(points[idx]);
        new_normals.push_back(normals[idx]);
    }

    points = std::move(new_points);
    normals = std::move(new_normals);
}

Mesh PointCloud::generate_mesh_gpu(unsigned int rings_count, unsigned int ring_size,  float rel_thresh) {
    size_t triangles_count = (rings_count - 1) * ring_size * 2;
    size_t vertices_count = triangles_count * 3;
    size_t vertex_ssbo_size = sizeof(Vertex) * vertices_count;
    size_t index_ssbo_size = sizeof(unsigned int) * vertices_count;

    
    update_points(points);
    sync_gpu();
    point_cloud_ssbo = new SSBO(*point_renderer.instance_vbo);
    vertex_ssbo = new SSBO(vertex_ssbo_size, GL_DYNAMIC_DRAW);
    // vertex_ssbo = new SSBO(sizeof(Vertex) * 3, GL_STATIC_DRAW);
    index_ssbo = new SSBO(index_ssbo_size, GL_DYNAMIC_DRAW);
    // index_ssbo = new SSBO(sizeof(unsigned int) * 3, GL_STATIC_DRAW);

    generate_mesh_gpu(*point_cloud_ssbo, *vertex_ssbo, *index_ssbo, rings_count, ring_size, rel_thresh);

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f, 1.0f});
    vertex_layout->add("normal", 1, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 4 * sizeof(float), 0, {0.0f, 1.0f, 0.0f, 0.0f});
    vertex_layout->add("color", 2, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), 8 * sizeof(float), 0, {1.0f, 1.0f, 1.0f, 1.0f});

    // std::vector<float> vertices(12 * 3);
    // std::vector<unsigned int> indices(3);

    // std::vector<float> vertices;
    // std::vector<unsigned int> indices;

    // PointInstance p0;
    // PointInstance p1;
    // PointInstance p2;

    // p0.pos = glm::vec3(0, 0, 0);
    // p1.pos = glm::vec3(0, 0, 5);
    // p2.pos = glm::vec3(5, 0, 5);

    // glm::vec3 color = glm::vec3(1, 1, 1);
    // glm::vec3 normal = glm::vec3(0, 1, 0);
    // push_point(vertices, p0, color, normal);
    // push_point(vertices, p1, color, normal);
    // push_point(vertices, p2, color, normal);

    // indices.push_back(0);
    // indices.push_back(1);
    // indices.push_back(2);
    // indices[0] = 0;
    // indices[1] = 1;
    // indices[2] = 2;

    // vertices.reserve(12 * 3);
    // vertex_ssbo->read_subdata(0, vertices.data(), sizeof(Vertex) * 3);

    // indices.reserve(3);
    // index_ssbo->read_subdata(0, indices.data(), sizeof(unsigned int) * 3);
    
    // std::vector<PointInstance> test_points(2);
    // point_cloud_ssbo->read_subdata(0, test_points.data(), sizeof(PointInstance) * test_points.size());

    // std::cout << test_points[0].pos.x << " " << test_points[0].pos.y << " " << test_points[0].pos.z << std::endl;

    // for (int i = 0; i < vertices.size(); i++)
    //     std::cout << vertices[i] << " ";
    // std::cout << std::endl;


    // Mesh mesh(vertices, indices, vertex_layout);
    Mesh mesh(*vertex_ssbo, *index_ssbo, vertex_layout);

    return mesh;
}

void PointCloud::generate_mesh_gpu(const SSBO& point_cloud_ssbo, const SSBO& vertex_ssbo, const SSBO& index_ssbo, 
                                   unsigned int rings_count, unsigned int ring_size, float rel_thresh) {
    point_cloud_ssbo.bind_base(0);
    vertex_ssbo.bind_base(1);
    index_ssbo.bind_base(2);

    // mesh_generation_program.set_int("rings_count", rings_count);
    // mesh_generation_program.set_int("rings_count", rings_count);
    

    int triangles_count = ring_size * 2;
    
    int x_count = math_utils::div_up_u32(triangles_count, 256u);;
    int y_count = rings_count - 1;

    mesh_generation_program.set_uint("triangles_count", triangles_count);

    GLuint q[2];
    glGenQueries(2, q);
    
    // glFinish();
    // double t0 = math_utils::ms_now();

    glQueryCounter(q[0], GL_TIMESTAMP);   
    mesh_generation_program.dispatch_compute(x_count, y_count, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glQueryCounter(q[1], GL_TIMESTAMP);   
    // glFinish();
    // double t1 = math_utils::ms_now();

    GLuint64 start_ns = 0, end_ns = 0;
    glGetQueryObjectui64v(q[0], GL_QUERY_RESULT, &start_ns);
    glGetQueryObjectui64v(q[1], GL_QUERY_RESULT, &end_ns);

    double gpu_ms = double(end_ns - start_ns) * 1e-6;
    // std::cout << "GPU time: " << gpu_ms << " ms\n";

    glDeleteQueries(2, q);

    // std::cout << t1 - t0 << " ms" << std::endl;
    // glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // Vertex out[3];
    // vertex_ssbo.read_subdata(0, out, sizeof(Vertex) * 3);

    // std::cout << sizeof(Vertex) << std::endl;

    // print_vertex(out[0]);
    // print_vertex(out[1]);
    // print_vertex(out[2]);

    // unsigned int out_indices[6];
    // index_ssbo.read_subdata(0, out_indices, sizeof(unsigned int) * 6);

    // for (int i = 0; i < 6; i++)
    //     std::cout << out_indices[i] << " ";
    // std::cout <<  std::endl;


    // PointInstance out_points[200];
    // point_cloud_ssbo.read_subdata(0, out_points, sizeof(PointInstance) * 200);

    // std::cout << out_points[150].pos.x << " " << out_points[150].pos.y << " " << out_points[150].pos.z << std::endl;
    // std::cout << points[150].pos.x << " " << points[150].pos.y << " " << points[150].pos.z << std::endl;

    // // for (int i = 0; i < 6; i++)
    // //     std::cout << out_points[i] << " ";
    // std::cout <<  std::endl;
}

void PointCloud::print_vertex(const Vertex& vertex) {
    std::cout << "position: ("<< vertex.position.x << ", " << vertex.position.y << ", " << vertex.position.z << ")";
    std::cout << "  normal: ("<< vertex.normal.x << ", " << vertex.normal.y << ", " << vertex.normal.z << ")";
    std::cout << "  color: ("<< vertex.color.x << ", " << vertex.color.y << ", " << vertex.color.z << ")" << std::endl;
}

void PointCloud::draw(RenderState state) {
    state.transform *= get_model_matrix();
    sync_gpu();
    point_renderer.draw(state);
}

