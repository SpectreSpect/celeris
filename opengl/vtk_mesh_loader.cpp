#include "vtk_mesh_loader.h"

VtkMeshLoader::VtkMeshLoader(VertexLayout& vertex_layout) {
    this->vertex_layout = vertex_layout;

    if (vertex_layout.attributes.size() > 0)
        default_vertex_data.reserve(vertex_layout.attributes[0].size / sizeof(float));
    
    for (VertexAttribute attribute : vertex_layout.attributes) {
        default_vertex_data.insert(
            default_vertex_data.end(), 
            attribute.default_value.begin(), 
            attribute.default_value.end()
        );
    }
    
    size_t position_attr_id = vertex_layout.find_attribute_id_by_name("position");
    size_t normal_attr_id = vertex_layout.find_attribute_id_by_name("normal");

    vertex_position_offset = vertex_layout.attributes[position_attr_id].offset / sizeof(float);
    vertex_normal_offset = vertex_layout.attributes[normal_attr_id].offset / sizeof(float);
}

void VtkMeshLoader::expect(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error("VTK parse error: " + msg);
}

MeshData VtkMeshLoader::load_mesh(std::string& file_path) const {
    std::ifstream in(file_path);
    if (!in) throw std::runtime_error("Failed to open file: " + file_path);

    MeshData mesh_data;

    std::string tok;

    bool dataset_ok = false;
    while (in >> tok) {
        if (tok == "DATASET") {
            std::string type;
            VtkMeshLoader::expect(read(in, type), "Expected dataset type after DATASET");
            VtkMeshLoader::expect(type == "POLYDATA", "Only DATASET POLYDATA is supported, got: " + type);
            dataset_ok = true;
            break;
        }
    }
    expect(dataset_ok, "DATASET not found");

    bool points_ok = false;
    int64_t num_points = 0;
    while (in >> tok) {
        if (tok == "POINTS") {
            std::string scalar_type;
            expect(read(in, num_points, scalar_type), "Bad POINTS header");
            expect(num_points >= 0, "Negative POINTS count");
            mesh_data.vertices.resize((size_t)num_points * default_vertex_data.size());

            for (int64_t i = 0; i < num_points; ++i) {
                float x, y, z;
                expect(read(in, x, y, z), "Not enough point coordinates");

                float* base = mesh_data.vertices.data() + (size_t)i * default_vertex_data.size();
                memcpy(base, default_vertex_data.data(), default_vertex_data.size() * sizeof(float));

                base[vertex_position_offset + 0] = y; 
                base[vertex_position_offset + 1] = z; 
                base[vertex_position_offset + 2] = x; 
            }
            points_ok = true;
            break;
        }
    }
    expect(points_ok, "POINTS section not found");

    bool polys_ok = false;
    int64_t num_polys = 0;
    int64_t total_ints = 0;

    while (in >> tok) {
        if (tok == "POLYGONS") {
            expect(read(in, num_polys, total_ints), "Bad POLYGONS header");
            expect(num_polys >= 0, "Negative POLYGONS count");

            mesh_data.indices.clear();
            mesh_data.indices.reserve((size_t)num_polys * 3);

            for (int64_t p = 0; p < num_polys; ++p) {
                int n = 0;
                expect(read(in, n), "Bad polygon (missing vertex count)");

                expect(n >= 0, "Negative polygon size");
                std::vector<uint32_t> poly;
                poly.resize((size_t)n);

                for (int i = 0; i < n; ++i) {
                    int64_t idx;
                    expect(read(in, idx), "Bad polygon index");
                    expect(idx >= 0 && idx < num_points, "Polygon index out of range");
                    poly[(size_t)i] = (uint32_t)idx;
                }

                if (n == 3) {
                    mesh_data.indices.push_back(poly[0]);
                    mesh_data.indices.push_back(poly[1]);
                    mesh_data.indices.push_back(poly[2]);
                } else if (n > 3) {
                    for (int i = 1; i + 1 < n; ++i) {
                        mesh_data.indices.push_back(poly[0]);
                        mesh_data.indices.push_back(poly[(size_t)i]);
                        mesh_data.indices.push_back(poly[(size_t)(i + 1)]);
                    }
                }
            }

            polys_ok = true;
            break;
        } else {
            // ignoring...
        }
    }
    expect(polys_ok, "POLYGONS section not found");

    size_t count_polygons = mesh_data.indices.size() / 3;
    for (size_t i = 0; i < count_polygons; i++) {
        size_t v_id1 = mesh_data.indices[i * 3 + 0];
        size_t v_id2 = mesh_data.indices[i * 3 + 1];
        size_t v_id3 = mesh_data.indices[i * 3 + 2];

        size_t vertex_base1 = v_id1 * default_vertex_data.size() + vertex_position_offset;
        size_t vertex_base2 = v_id2 * default_vertex_data.size() + vertex_position_offset;
        size_t vertex_base3 = v_id3 * default_vertex_data.size() + vertex_position_offset;

        size_t normal_base1 = v_id1 * default_vertex_data.size() + vertex_normal_offset;
        size_t normal_base2 = v_id2 * default_vertex_data.size() + vertex_normal_offset;
        size_t normal_base3 = v_id3 * default_vertex_data.size() + vertex_normal_offset;

        glm::vec3 v0 = glm::vec3(mesh_data.vertices[vertex_base1 + 0], mesh_data.vertices[vertex_base1 + 1], mesh_data.vertices[vertex_base1 + 2]);
        glm::vec3 v1 = glm::vec3(mesh_data.vertices[vertex_base2 + 0], mesh_data.vertices[vertex_base2 + 1], mesh_data.vertices[vertex_base2 + 2]);
        glm::vec3 v2 = glm::vec3(mesh_data.vertices[vertex_base3 + 0], mesh_data.vertices[vertex_base3 + 1], mesh_data.vertices[vertex_base3 + 2]);

        glm::vec3 e1 = v1 - v0;
        glm::vec3 e2 = v2 - v0;
        glm::vec3 normal  = glm::normalize(glm::cross(e1, e2));
        float normal_arr[3] = {normal.x, normal.y, normal.z}; 

        memcpy(mesh_data.vertices.data() + normal_base1, normal_arr, sizeof(normal_arr));
        memcpy(mesh_data.vertices.data() + normal_base2, normal_arr, sizeof(normal_arr));
        memcpy(mesh_data.vertices.data() + normal_base3, normal_arr, sizeof(normal_arr));
    }
    
    return mesh_data;
}